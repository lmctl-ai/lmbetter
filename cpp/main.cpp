// lmok: tree-sitter's incremental parsing engine as one statically
// linked, scriptable CLI. Host for the lmscript DSL plus a small parse
// subcommand (JSON grammar).
#include "host.hpp"
#include "lint.hpp"
#include "input.hpp"
#include "dslvm.hpp"
#include "stdlib/string.hpp"
#include "stdlib/collections.hpp"
#include "schema_embedded.hpp"
#include "notices_embedded.hpp"

#include <cstdio>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <map>
#include <unordered_map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

namespace {
using dslvm::Args;
using dslvm::Value;

// ---- arena for schema object handles ----
struct Finding {
  int64_t row, col, end_row, end_col; // 0-based, tree-sitter convention
  std::string message;
  // Optional rule metadata (4-arg ts.report overload). Free-form strings —
  // severity policy belongs to the script, the host validates nothing.
  std::string rule, severity;
};

// Every arena is append-only for one program run and cleared by reset()
// before the next --dsl file (like the SDK's per-run reset of standard
// collection handles), so handles never dangle and the caps are per run, not
// per process. Trees and NodeLists are few (65536); Node handles are many
// (one per root()/child()/parent() call; NodeList.at() hands out pointers
// into the list itself and allocates nothing), and a Node is ~40 bytes, so
// their cap is 1<<20 (~40 MB worst case) in a deque (stable addresses, no
// per-node heap allocation).
struct Host {
  static constexpr size_t kHandleCap = 65536;
  static constexpr size_t kNodeCap = size_t(1) << 20;
  std::vector<std::unique_ptr<lmok::Tree>> trees;
  std::deque<lmok::Node> nodes;
  // One handle per distinct node per run (WP14 F4): a rule that climbs n.parent() for every node used to
  // allocate depth^2 handles and hit kNodeCap on inputs nested a few thousand deep; keyed by tree-sitter's
  // node identity, so repeated root()/child()/parent() answers reuse the same stable handle.
  std::unordered_map<const void *, lmok::Node *> node_index;
  // Parent of every node a rule can reach by climbing: filled from the walk's ancestor path when a node is
  // dispatched (each ancestor once per file), so Node.parent()/has_parent() are O(1) map hits instead of
  // tree-sitter's O(depth) ts_node_parent, which made nesting-depth rules cubic on deep inputs (WP14 F4).
  std::unordered_map<const void *, lmok::Node> parent_of;
  bool remember_parent(const lmok::Node &child, const lmok::Node &parent) {
    return parent_of.emplace(lmok::node_id(child), parent).second;
  }
  const lmok::Node *cached_parent(const lmok::Node &node) const {
    const auto it = parent_of.find(lmok::node_id(node));
    return it == parent_of.end() ? nullptr : &it->second;
  }
  std::vector<std::unique_ptr<lmok::NodeList>> node_lists;
  // Findings collected via ts.report; reset per --dsl file (see the run loop).
  std::vector<Finding> findings;
  template <class T>
  Value hold(std::vector<std::unique_ptr<T>> &arena, const char *type, T value) {
    if (arena.size() >= kHandleCap)
      throw dslvm::Error(std::string(type) + " object limit exceeded (" + std::to_string(kHandleCap) + ")");
    arena.push_back(std::make_unique<T>(std::move(value)));
    return Value::object(type, arena.back().get());
  }
  Value hold_node(lmok::Node node) {
    const auto known = node_index.find(lmok::node_id(node));
    if (known != node_index.end()) return Value::object("Node", known->second);
    if (nodes.size() >= kNodeCap)
      throw dslvm::Error("Node object limit exceeded (" + std::to_string(kNodeCap) + ")");
    nodes.push_back(node);
    node_index.emplace(lmok::node_id(nodes.back()), &nodes.back());
    return Value::object("Node", &nodes.back());
  }
  void reset() {
    trees.clear();
    nodes.clear();
    node_index.clear();
    parent_of.clear();
    node_lists.clear();
    findings.clear();
  }
};

template <class T> T *self(void *p) { return static_cast<T *>(p); }

lmok::Tree *tree_of(void *p, const char *who) {
  auto *t = self<lmok::Tree>(p);
  if (!t) throw dslvm::Error(std::string(who) + ": stale Tree handle");
  return t;
}

lmok::Node *node_of(void *p, const char *who) {
  auto *n = self<lmok::Node>(p);
  if (!n) throw dslvm::Error(std::string(who) + ": stale Node handle");
  return n;
}

lmok::NodeList *nodelist_of(void *p, const char *who) {
  auto *l = self<lmok::NodeList>(p);
  if (!l) throw dslvm::Error(std::string(who) + ": stale NodeList handle");
  return l;
}

// The CLI has UTF-8 paths; leave the pinned SDK reader unchanged.
std::string read_bytecode(const std::string &path) {
  try { return lmok::read_input_file(path); }
  catch (const dslvm::Error &e) { throw dslvm::Error("LOAD_BAD_BYTECODE", e.what()); }
}

std::string read_file(const std::string &path) {
  return lmok::read_input_file(path);
}

// JSON string escaping for ts.report_json output.
std::string json_escape(const std::string &s) {
  std::string out;
  for (size_t i = 0; i < s.size(); ++i) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    if (c >= 0x80) {
      size_t length = c >= 0xc2 && c <= 0xdf ? 2 :
                      c >= 0xe0 && c <= 0xef ? 3 :
                      c >= 0xf0 && c <= 0xf4 ? 4 : 0;
      bool valid = length && length <= s.size() - i;
      for (size_t j = 1; valid && j < length; ++j) {
        const unsigned char next = static_cast<unsigned char>(s[i + j]);
        valid = next >= 0x80 && next <= 0xbf;
        if (j == 1 && ((c == 0xe0 && next < 0xa0) || (c == 0xed && next >= 0xa0) ||
                       (c == 0xf0 && next < 0x90) || (c == 0xf4 && next >= 0x90))) valid = false;
      }
      if (valid) { out.append(s, i, length); i += length - 1; }
      else out += "\\ufffd"; // malformed source bytes must not corrupt JSON output
      continue;
    }
    switch (c) {
    case '"': out += "\\\""; break;
    case '\\': out += "\\\\"; break;
    case '\n': out += "\\n"; break;
    case '\r': out += "\\r"; break;
    case '\t': out += "\\t"; break;
    default:
      if (c < 0x20) {
        char buf[8];
        std::snprintf(buf, sizeof buf, "\\u%04x", c);
        out += buf;
      } else out += static_cast<char>(c);
    }
  }
  return out;
}

// One JSON representation for script reports and host-dispatched findings.
// IDs/severity come from ts.report, never from the entry's dispatch name.
std::string findings_json(const std::vector<Finding> &findings) {
    std::ostringstream out;
    out << "[";
    bool first = true;
    for (const auto &f : findings) {
      if (!first) out << ",";
      first = false;
      out << "{\"row\":" << f.row << ",\"col\":" << f.col
          << ",\"end_row\":" << f.end_row << ",\"end_col\":" << f.end_col;
      if (!f.rule.empty()) out << ",\"rule\":\"" << json_escape(f.rule) << "\"";
      if (!f.severity.empty()) out << ",\"severity\":\"" << json_escape(f.severity) << "\"";
      out << ",\"message\":\"" << json_escape(f.message) << "\"}";
    }
    out << "]";
    return out.str();
}

// ---- DSL registry: the schema implementation ----
void register_host(dslvm::Registry &reg, Host &host) {
  reg.context("ts", "Ts");

  reg.function("println", {"string"}, "void", [](Args &args) {
    std::cout << args[0].s << "\n";
    return Value::none();
  });

  // The "ts" context object (type Ts) is the parse entry point: the DSL
  // compiler resolves ts.parse(...) as a method call on the context object.
  const auto parse_method = [&reg, &host](const char *name, lmok::Grammar grammar) {
    reg.method("Ts", name, {"string"}, "Tree", [&host, grammar](void *, Args &args) {
      try {
        return host.hold(host.trees, "Tree", lmok::parse(grammar, args[0].s));
      } catch (const std::length_error &error) {
        throw dslvm::Error(error.what());
      }
    });
  };
  parse_method("parse", lmok::Grammar::Json);
  parse_method("parse_c", lmok::Grammar::C);
  parse_method("parse_python", lmok::Grammar::Python);
  parse_method("parse_bash", lmok::Grammar::Bash);
  // The findings sink: rules are void fns, so they report into the host and
  // the chunk body reads the collection back (count / JSON array string).
  const auto report_impl = [&host](void *, Args &args) {
    if (host.findings.size() >= 65536)
      throw dslvm::Error("report limit exceeded (65536)");
    const lmok::Node *n = node_of(args[0].obj, "report");
    Finding f{lmok::node_start_row(*n), lmok::node_start_col(*n),
              lmok::node_end_row(*n), lmok::node_end_col(*n), args[1].s, "", ""};
    if (args.size() == 4) { f.rule = args[2].s; f.severity = args[3].s; }
    host.findings.push_back(std::move(f));
    return Value::none();
  };
  reg.method("Ts", "report", {"Node", "string"}, "void", report_impl);
  reg.method("Ts", "report", {"Node", "string", "string", "string"}, "void", report_impl);
  reg.method("Ts", "report_count", {}, "int", [&host](void *, Args &) {
    return Value::integer(static_cast<int64_t>(host.findings.size()));
  });
  reg.method("Ts", "report_json", {}, "string", [&host](void *, Args &) {
    return Value::string(findings_json(host.findings));
  });
  const auto query_method = [&reg, &host](const char *name, lmok::Grammar grammar) {
    reg.method("Ts", name, {"Node", "string"}, "NodeList", [&host, grammar](void *, Args &args) {
      try {
        return host.hold(host.node_lists, "NodeList",
                         lmok::node_query(*node_of(args[0].obj, "query"), args[1].s, grammar));
      } catch (const lmok::QueryError &error) {
        throw dslvm::Error(error.what());
      }
    });
  };
  query_method("query", lmok::Grammar::Json);
  query_method("query_c", lmok::Grammar::C);
  query_method("query_python", lmok::Grammar::Python);
  query_method("query_bash", lmok::Grammar::Bash);
  const auto reparse_method = [&reg, &host](const char *name, lmok::Grammar grammar) {
    reg.method("Ts", name, {"Tree", "string"}, "Tree", [&host, grammar](void *, Args &args) {
      try {
        return host.hold(host.trees, "Tree",
                         lmok::reparse(grammar, *tree_of(args[0].obj, "reparse"), args[1].s));
      } catch (const std::out_of_range &error) {
        throw dslvm::Error(error.what());
      } catch (const std::length_error &error) {
        throw dslvm::Error(error.what());
      }
    });
  };
  reparse_method("reparse", lmok::Grammar::Json);
  reparse_method("reparse_c", lmok::Grammar::C);
  reparse_method("reparse_python", lmok::Grammar::Python);
  reparse_method("reparse_bash", lmok::Grammar::Bash);
  reg.method("Ts", "changed_ranges", {"Tree", "Tree"}, "string", [](void *, Args &args) {
    return Value::string(lmok::changed_ranges_json(*tree_of(args[0].obj, "changed_ranges"),
                                                       *tree_of(args[1].obj, "changed_ranges")));
  });

  reg.method("Tree", "edit", {"int", "int", "int"}, "void", [](void *s, Args &args) {
    try {
      lmok::tree_edit(*tree_of(s, "edit"), args[0].i, args[1].i, args[2].i);
    } catch (const std::out_of_range &error) {
      throw dslvm::Error(error.what());
    }
    return Value::none();
  });

  reg.method("Tree", "sexp", {}, "string", [](void *s, Args &) {
    return Value::string(lmok::tree_sexp(*tree_of(s, "sexp")));
  });
  reg.method("Tree", "has_error", {}, "bool", [](void *s, Args &) {
    return Value::boolean(lmok::tree_has_error(*tree_of(s, "has_error")));
  });
  reg.method("Tree", "node_count", {}, "int", [](void *s, Args &) {
    return Value::integer(lmok::tree_node_count(*tree_of(s, "node_count")));
  });
  reg.method("Tree", "root", {}, "Node", [&host](void *s, Args &) {
    return host.hold_node(lmok::node_root(*tree_of(s, "root")));
  });
  reg.method("Tree", "nodes", {}, "NodeList", [&host](void *s, Args &) {
    return host.hold(host.node_lists, "NodeList", lmok::tree_nodes(*tree_of(s, "nodes")));
  });

  const auto node_string_method = [&reg](const char *name, auto impl) {
    reg.method("Node", name, {}, "string", [name, impl](void *s, Args &) {
      return Value::string(impl(node_of(s, name)));
    });
  };
  node_string_method("kind", [](const lmok::Node *n) { return lmok::node_kind(*n); });
  node_string_method("text", [](const lmok::Node *n) { return lmok::node_text(*n); });
  reg.method("Node", "is_named", {}, "bool", [](void *s, Args &) {
    return Value::boolean(lmok::node_is_named(*node_of(s, "is_named")));
  });
  reg.method("Node", "is_missing", {}, "bool", [](void *s, Args &) {
    return Value::boolean(lmok::node_is_missing(*node_of(s, "is_missing")));
  });
  const auto node_point_method = [&reg](const char *name, auto impl) {
    reg.method("Node", name, {}, "int", [name, impl](void *s, Args &) {
      return Value::integer(impl(node_of(s, name)));
    });
  };
  node_point_method("start_row", [](const lmok::Node *n) { return lmok::node_start_row(*n); });
  node_point_method("start_col", [](const lmok::Node *n) { return lmok::node_start_col(*n); });
  node_point_method("end_row", [](const lmok::Node *n) { return lmok::node_end_row(*n); });
  node_point_method("end_col", [](const lmok::Node *n) { return lmok::node_end_col(*n); });
  reg.method("Node", "child_count", {}, "int", [](void *s, Args &) {
    return Value::integer(lmok::node_child_count(*node_of(s, "child_count")));
  });
  reg.method("Node", "child", {"int"}, "Node", [&host](void *s, Args &args) {
    const lmok::Node *n = node_of(s, "child");
    const int64_t count = lmok::node_child_count(*n);
    if (args[0].i < 0 || args[0].i >= count)
      throw dslvm::Error("Node.child index " + std::to_string(args[0].i) +
                         " out of range (named child count " + std::to_string(count) + ")");
    return host.hold_node(lmok::node_child(*n, args[0].i));
  });
  reg.method("Node", "has_parent", {}, "bool", [&host](void *s, Args &) {
    const lmok::Node *n = node_of(s, "has_parent");
    return Value::boolean(host.cached_parent(*n) != nullptr || lmok::node_has_parent(*n));
  });
  reg.method("Node", "parent", {}, "Node", [&host](void *s, Args &) {
    const lmok::Node *n = node_of(s, "parent");
    if (const lmok::Node *cached = host.cached_parent(*n)) return host.hold_node(*cached);
    if (!lmok::node_has_parent(*n))
      throw dslvm::Error("Node.parent: root node has no parent");
    return host.hold_node(lmok::node_parent(*n));
  });

  reg.method("NodeList", "len", {}, "int", [](void *s, Args &) {
    return Value::integer(static_cast<int64_t>(nodelist_of(s, "len")->items.size()));
  });
  reg.method("NodeList", "at", {"int"}, "Node", [](void *s, Args &args) {
    lmok::NodeList *l = nodelist_of(s, "at");
    const int64_t count = static_cast<int64_t>(l->items.size());
    if (args[0].i < 0 || args[0].i >= count)
      throw dslvm::Error("NodeList.at index " + std::to_string(args[0].i) +
                         " out of range (length " + std::to_string(count) + ")");
    // The list lives in the arena for the run and its vector is never
    // resized, so a pointer into it is a stable Node handle: iterating a
    // 200k-node tree allocates nothing (and no longer burns the Node cap).
    return Value::object("Node", &l->items[static_cast<size_t>(args[0].i)]);
  });
}

void print_schema_summary(const dslvm::Registry &reg) {
  for (const auto &line : reg.signatures()) std::cout << line << "\n";
}
} // namespace

static int run_cli(int argc, char **argv) {
#ifdef _WIN32
  _setmode(_fileno(stdin), _O_BINARY); // `parse -` must not CRLF-translate or stop at ^Z
  _setmode(_fileno(stdout), _O_BINARY);
  _setmode(_fileno(stderr), _O_BINARY);
#endif
  Host host;
  dslvm::Registry reg;
  register_host(reg, host);
  const std::string usage =
      "usage: lmok --version | --notices | --dsl-schema [--json] | --dsl-signatures\n"
      "       lmok parse [--lang json|c|python|bash] FILE\n"
      "       lmok query [--lang json|c|python|bash] PATTERN FILE\n"
      "       lmok lint RULES.json FILE...\n"
      "       lmok check [--lang json|c|python|bash] RULES.dslb FILE...\n"
      "       lmok --dsl FILE [FILE ...]\n";
  if (argc == 2 && std::string(argv[1]) == "--version") {
    std::cout << "lmok " LMOK_VERSION "\n";
    return std::cout ? 0 : 4;
  }
  if (argc == 2 && std::string(argv[1]) == "--notices") {
    std::cout.write(reinterpret_cast<const char *>(lmok::embedded_notices),
                    static_cast<std::streamsize>(lmok::embedded_notices_size));
    return std::cout ? 0 : 4;
  }
  if (argc >= 2 && std::string(argv[1]) == "--dsl-schema") {
    if (argc == 2) {
      print_schema_summary(reg);
      return std::cout ? 0 : 4;
    }
    if (argc == 3 && std::string(argv[2]) == "--json") {
      std::cout.write(reinterpret_cast<const char *>(lmok::embedded_schema),
                      static_cast<std::streamsize>(lmok::embedded_schema_size));
      return std::cout ? 0 : 4;
    }
    std::cerr << usage;
    return 2;
  }
  if (argc == 2 && std::string(argv[1]) == "--dsl-signatures") {
    for (const auto &signature : reg.signatures()) std::cout << signature << "\n";
    return 0;
  }
  if (argc >= 2 && (std::string(argv[1]) == "parse" || std::string(argv[1]) == "query")) {
    const bool is_query = std::string(argv[1]) == "query";
    lmok::Grammar grammar = lmok::Grammar::Json;
    int argi = 2;
    if (argi < argc && std::string(argv[argi]) == "--lang" && argi + 1 >= argc) {
      std::cerr << "--lang needs a value (json|c)\n" << usage;
      return 2;
    }
    if (argi + 1 < argc && std::string(argv[argi]) == "--lang") {
      const std::string lang = argv[argi + 1];
      if (lang == "json") grammar = lmok::Grammar::Json;
      else if (lang == "c") grammar = lmok::Grammar::C;
      else if (lang == "python") grammar = lmok::Grammar::Python;
      else if (lang == "bash") grammar = lmok::Grammar::Bash;
      else {
        std::cerr << "unknown language '" << lang << "' (known: json, c, python, bash)\n" << usage;
        return 2;
      }
      argi += 2;
    }
    const char *pattern = nullptr;
    if (is_query) {
      if (argi >= argc) { std::cerr << usage; return 2; }
      pattern = argv[argi++];
    }
    if (argi != argc - 1) { std::cerr << usage; return 2; }
    std::string raw;
    const std::string path = argv[argi];
    if (path == "-") {
      std::ostringstream data;
      data << std::cin.rdbuf();
      raw = data.str();
    } else {
      try { raw = read_file(path); }
      catch (const dslvm::Error &e) { std::cerr << e.what() << "\n"; return 3; }
    }
    lmok::Tree tree;
    try { tree = lmok::parse(grammar, raw); }
    catch (const std::length_error &e) { std::cerr << e.what() << "\n"; return 3; }
    if (!is_query) {
      std::cout << lmok::tree_sexp(tree) << "\n";
      return std::cout ? 0 : 4;
    }
    lmok::NodeList captures;
    try {
      captures = lmok::node_query(lmok::node_root(tree), pattern, grammar);
    } catch (const lmok::QueryError &e) {
      std::cerr << e.what() << "\n";
      return 3;
    }
    for (const auto &node : captures.items)
      std::cout << lmok::node_kind(node) << " "
                << lmok::node_start_row(node) << ":" << lmok::node_start_col(node)
                << "-" << lmok::node_end_row(node) << ":" << lmok::node_end_col(node) << "\n";
    return std::cout ? 0 : 4;
  }
  if (argc >= 2 && std::string(argv[1]) == "lint") {
    if (argc < 4) { std::cerr << usage; return 2; }
    std::vector<lmok::LintRule> rules;
    try {
      rules = lmok::load_rule_pack(argv[2]);
    } catch (const dslvm::Error &e) {
      std::cerr << e.what() << "\n";
      return 2;
    }
    // Per-grammar rule lists for extension-based filtering.
    std::vector<lmok::LintRule> c_rules, json_rules, python_rules, bash_rules;
    for (const auto &rule : rules) {
      if (rule.grammar == lmok::Grammar::C) c_rules.push_back(rule);
      else if (rule.grammar == lmok::Grammar::Python) python_rules.push_back(rule);
      else if (rule.grammar == lmok::Grammar::Bash) bash_rules.push_back(rule);
      else json_rules.push_back(rule);
    }
    // Directory args expand RECURSIVELY to their regular files: hidden files
    // and dirs are skipped, only known extensions (.c/.h -> c, .json -> json,
    // .py -> python, .sh/.bash -> bash) are lintable, and paths are sorted for
    // byte-stable output. An explicit file keeps its argument order.
    // Output canonicalization: all printed paths use forward slashes
    // (fs::path::generic_u8string), on every OS — pinned outputs are
    // platform-independent.
    namespace fs = std::filesystem;
    const auto canonical = [](const std::string &path) {
      return fs::u8path(path).generic_u8string();
    };
    struct Target { std::string path; bool from_dir; };
    std::vector<Target> targets;
    for (int i = 3; i < argc; ++i) {
      std::error_code ec;
      const fs::path root = fs::u8path(argv[i]);
      if (fs::is_directory(root, ec)) {
        std::vector<std::string> found;
        const fs::recursive_directory_iterator end;
        // Iterator errors (an unreadable subdirectory, a vanished entry) must
        // not end the walk silently with a partial file list: that is the
        // same fail-fast contract as an unreadable file (exit 3).
        fs::recursive_directory_iterator it(root, ec);
        fs::path failed_at = root;
        for (; !ec && it != end; failed_at = it->path(), it.increment(ec)) {
          const fs::path &p = it->path();
          // Hidden check FIRST: directories must be pruned (never descended
          // into) before the regular-file filter would skip them.
          // Hidden means a dot-component BELOW the argument root: a project
          // living under ~/.local or passed as ./.proj is still lintable.
          bool hidden = false;
          for (const auto &part : p.lexically_relative(root)) {
            const std::string name = part.u8string();
            if (name != "." && name != ".." && name.rfind(".", 0) == 0) hidden = true; // C++17: no starts_with
          }
          if (hidden) { it.disable_recursion_pending(); continue; } // prune hidden dirs
          std::error_code file_ec;
          if (!it->is_regular_file(file_ec)) continue;
          const std::string ext = p.extension().u8string();
          if (ext == ".c" || ext == ".h" || ext == ".json" || ext == ".py" ||
              ext == ".sh" || ext == ".bash") found.push_back(canonical(p.u8string()));
        }
        if (ec) {
          std::cerr << "cannot read " << canonical(failed_at.u8string()) << ": " << ec.message() << "\n";
          return 3;
        }
        if (found.empty()) {
          std::cerr << "no lintable files in " << canonical(argv[i]) << " (want .c/.h/.json/.py/.sh/.bash)\n";
          return 2;
        }
        std::sort(found.begin(), found.end());
        for (auto &f : found) targets.push_back({f, true});
      } else {
        targets.push_back({canonical(argv[i]), false});
      }
    }
    // Fail fast: the first unreadable file aborts the whole run (exit 3);
    // earlier files' findings were already printed.
    for (const auto &target : targets) {
      const std::vector<lmok::LintRule> *active = &rules;
      const std::string ext = fs::u8path(target.path).extension().u8string();
      if (ext == ".c" || ext == ".h") active = &c_rules;
      else if (ext == ".json") active = &json_rules;
      else if (ext == ".py") active = &python_rules;
      else if (ext == ".sh" || ext == ".bash") active = &bash_rules;
      // A file whose language has no rules in the pack: skipped silently when
      // it came from a directory; an explicitly named file still reports.
      if (active->empty() && target.from_dir) continue;
      std::string raw;
      try { raw = read_file(target.path); }
      catch (const dslvm::Error &e) { std::cerr << e.what() << "\n"; return 3; }
      std::vector<lmok::LintFinding> findings;
      try {
        findings = lmok::lint_text(*active, raw);
      } catch (const lmok::QueryError &e) {
        std::cerr << e.what() << "\n";
        return 3;
      } catch (const std::exception &e) { // oversized input, regex engine limits, allocation
        std::cerr << "lint " << target.path << ": " << e.what() << "\n";
        return 3;
      }
      for (const auto &f : findings)
        std::cout << "{\"row\":" << f.row << ",\"col\":" << f.col
                  << ",\"end_row\":" << f.end_row << ",\"end_col\":" << f.end_col
                  << ",\"rule\":\"" << json_escape(f.rule)
                  << "\",\"severity\":\"" << json_escape(f.severity)
                  << "\",\"message\":\"" << json_escape(f.message) << "\"}\n";
      std::cout << findings.size() << " findings in " << target.path << "\n";
    }
    return std::cout ? 0 : 4;
  }
  if (argc >= 2 && std::string(argv[1]) == "check") {
    int argi = 2;
    lmok::Grammar grammar = lmok::Grammar::Json;
    if (argi + 1 < argc && std::string(argv[argi]) == "--lang") {
      const std::string lang = argv[argi + 1];
      if (lang == "json") grammar = lmok::Grammar::Json;
      else if (lang == "c") grammar = lmok::Grammar::C;
      else if (lang == "python") grammar = lmok::Grammar::Python;
      else if (lang == "bash") grammar = lmok::Grammar::Bash;
      else { std::cerr << "unknown language '" << lang << "' (known: json, c, python, bash)\n"; return 2; }
      argi += 2;
    }
    if (argi + 1 >= argc) { std::cerr << usage; return 2; }
    for (int i = argi; i < argc; ++i) {
      if (std::string(argv[i]).empty() || std::string(argv[i]).rfind("--", 0) == 0) { std::cerr << usage; return 2; }
    }
    const std::string schema(reinterpret_cast<const char *>(lmok::embedded_schema), lmok::embedded_schema_size);
    dslvm::Program program;
    try { program = dslvm::Program::load(read_bytecode(argv[argi++]), schema, reg); }
    catch (const dslvm::Error &error) { return dslvm::report_error("load", error); }
    std::map<std::string, std::vector<size_t>> by_kind;
    for (size_t i = 0; i < program.entries().size(); ++i) {
      const auto &entry = program.entries()[i];
      if (entry.iface != "interface Rule(Node) -> void") {
        return dslvm::report_error("load", dslvm::Error("LOAD_BAD_BYTECODE", "check entry '" + entry.name + "' must implement Rule(Node) -> void"));
      }
      by_kind[entry.annotation].push_back(i); // duplicate kinds retain source order
    }
    dslvm::Context context;
    context.set("ts", &host);
    try {
      program.run(context, reg); // configuration/effects once, not once per node or file
      for (; argi < argc; ++argi) {
        host.reset(); // transient body/file objects and findings do not escape an input
        const lmok::Tree tree = lmok::parse(grammar, read_file(argv[argi]));
        lmok::walk_nodes(tree, [&](const lmok::Node &node, const std::vector<lmok::Node> &ancestors) {
          const auto found = by_kind.find(lmok::node_kind(node));
          if (found == by_kind.end()) return;
          // Record the ancestor chain up to the first already-known link: every ancestor is stored once per file.
          const lmok::Node *child = &node;
          for (size_t k = ancestors.size(); k-- > 0; child = &ancestors[k]) if (!host.remember_parent(*child, ancestors[k])) break;
          const Value handle = host.hold_node(node); // per-file arena; Tree stays alive
          for (size_t entry : found->second) program.call(entry, context, reg, {handle});
        });
        std::cout << findings_json(host.findings) << "\n" << host.findings.size() << " findings\n";
        dslvm::report_ok();
        if (!std::cout) return 4;
      }
    } catch (const dslvm::Error &error) { return dslvm::report_error("run", error); }
      catch (const std::exception &error) { return dslvm::report_error("run", dslvm::Error("RUN_HOST", error.what())); }
    return 0;
  }
  if (argc >= 3 && std::string(argv[1]) == "--dsl") {
    for (int i = 2; i < argc; ++i) {
      if (std::string(argv[i]).empty() || std::string(argv[i]).rfind("--", 0) == 0) {
        std::cerr << usage;
        return 2;
      }
    }
    const std::string schema(reinterpret_cast<const char *>(lmok::embedded_schema),
                             lmok::embedded_schema_size);
    dslvm::Context context;
    context.set("ts", &host);
    std::vector<dslvm::Program> programs;
    for (int i = 2; i < argc; ++i) {
      try {
        programs.push_back(dslvm::Program::load(read_bytecode(argv[i]), schema, reg));
      } catch (const dslvm::Error &error) {
        return dslvm::report_error("load", error);
      }
    }
    for (const auto &program : programs) {
      host.reset(); // trees, nodes, lists and findings are per-file state (caps are per run)
      try {
        program.run(context, reg);
      } catch (const dslvm::Error &error) {
        return dslvm::report_error("run", error);
      } catch (const std::exception &error) {
        // Anything the host did not translate (regex engine limits,
        // allocation failure) still ends with the standard result line
        // instead of std::terminate.
        return dslvm::report_error("run", dslvm::Error("RUN_HOST", error.what()));
      }
      dslvm::report_ok();
    }
    return 0;
  }
  std::cerr << usage;
  return 2;
}

#ifdef _WIN32
// The narrow CRT argv uses the Windows ANSI code page and loses Unicode paths.
// Keep the CLI/DSL boundary UTF-8, converting only at the native entry point.
int wmain(int argc, wchar_t **wide) {
  std::vector<std::string> text;
  text.reserve(argc);
  for (int i = 0; i < argc; ++i) {
    int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide[i], -1, nullptr, 0, nullptr, nullptr);
    if (size == 0) { std::cerr << "invalid Unicode command argument\n"; return 2; }
    std::string value(static_cast<size_t>(size), '\0');
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide[i], -1, value.data(), size, nullptr, nullptr)) {
      std::cerr << "invalid Unicode command argument\n"; return 2;
    }
    value.pop_back();
    text.push_back(std::move(value));
  }
  std::vector<char *> argv;
  argv.reserve(static_cast<size_t>(argc) + 1);
  for (auto &value : text) argv.push_back(value.data());
  argv.push_back(nullptr);
  return run_cli(argc, argv.data());
}
#else
int main(int argc, char **argv) { return run_cli(argc, argv); }
#endif
