// lmbetterp: tree-sitter's incremental parsing engine as one statically
// linked, scriptable CLI. Host for the lmscript DSL plus a small parse
// subcommand (JSON grammar).
#include "host.hpp"
#include "dslvm.hpp"
#include "stdlib/string.hpp"
#include "stdlib/collections.hpp"
#include "schema_embedded.hpp"
#include "notices_embedded.hpp"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

namespace {
using dslvm::Args;
using dslvm::Value;

// ---- arena for schema object handles ----
struct Host {
  std::vector<std::unique_ptr<lmbetterp::Tree>> trees;
  template <class T>
  Value hold(std::vector<std::unique_ptr<T>> &arena, const char *type, T value) {
    if (arena.size() >= 65536)
      throw dslvm::Error(std::string(type) + " object limit exceeded (65536)");
    arena.push_back(std::make_unique<T>(std::move(value)));
    return Value::object(type, arena.back().get());
  }
};

template <class T> T *self(void *p) { return static_cast<T *>(p); }

lmbetterp::Tree *tree_of(void *p, const char *who) {
  auto *t = self<lmbetterp::Tree>(p);
  if (!t) throw dslvm::Error(std::string(who) + ": stale Tree handle");
  return t;
}

std::string read_file(const std::string &path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw dslvm::Error("cannot read " + path);
  std::ostringstream data;
  data << in.rdbuf();
  if (in.bad()) throw dslvm::Error("cannot read " + path);
  return data.str();
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
  reg.method("Ts", "parse", {"string"}, "Tree", [&host](void *, Args &args) {
    return host.hold(host.trees, "Tree", lmbetterp::parse_json(args[0].s));
  });

  reg.method("Tree", "sexp", {}, "string", [](void *s, Args &) {
    return Value::string(lmbetterp::tree_sexp(*tree_of(s, "sexp")));
  });
  reg.method("Tree", "has_error", {}, "bool", [](void *s, Args &) {
    return Value::boolean(lmbetterp::tree_has_error(*tree_of(s, "has_error")));
  });
  reg.method("Tree", "node_count", {}, "int", [](void *s, Args &) {
    return Value::integer(lmbetterp::tree_node_count(*tree_of(s, "node_count")));
  });
}

void print_schema_summary(const dslvm::Registry &reg) {
  for (const auto &line : reg.signatures()) std::cout << line << "\n";
}
} // namespace

int main(int argc, char **argv) {
#ifdef _WIN32
  _setmode(_fileno(stdout), _O_BINARY);
  _setmode(_fileno(stderr), _O_BINARY);
#endif
  Host host;
  dslvm::Registry reg;
  register_host(reg, host);
  const std::string usage =
      "usage: lmbetterp --version | --notices | --dsl-schema [--json] | --dsl-signatures\n"
      "       lmbetterp parse FILE\n"
      "       lmbetterp --dsl FILE [FILE ...]\n";
  if (argc == 2 && std::string(argv[1]) == "--version") {
    std::cout << "lmbetterp " LMBETTERP_VERSION "\n";
    return std::cout ? 0 : 4;
  }
  if (argc == 2 && std::string(argv[1]) == "--notices") {
    std::cout.write(reinterpret_cast<const char *>(lmbetterp::embedded_notices),
                    static_cast<std::streamsize>(lmbetterp::embedded_notices_size));
    return std::cout ? 0 : 4;
  }
  if (argc >= 2 && std::string(argv[1]) == "--dsl-schema") {
    if (argc == 2) {
      print_schema_summary(reg);
      return std::cout ? 0 : 4;
    }
    if (argc == 3 && std::string(argv[2]) == "--json") {
      std::cout.write(reinterpret_cast<const char *>(lmbetterp::embedded_schema),
                      static_cast<std::streamsize>(lmbetterp::embedded_schema_size));
      return std::cout ? 0 : 4;
    }
    std::cerr << usage;
    return 2;
  }
  if (argc == 2 && std::string(argv[1]) == "--dsl-signatures") {
    for (const auto &signature : reg.signatures()) std::cout << signature << "\n";
    return 0;
  }
  if (argc == 3 && std::string(argv[1]) == "parse") {
    std::string raw;
    const std::string path = argv[2];
    if (path == "-") {
      std::ostringstream data;
      data << std::cin.rdbuf();
      raw = data.str();
    } else {
      try { raw = read_file(path); }
      catch (const dslvm::Error &e) { std::cerr << e.what() << "\n"; return 3; }
    }
    const lmbetterp::Tree tree = lmbetterp::parse_json(raw);
    std::cout << lmbetterp::tree_sexp(tree) << "\n";
    return std::cout ? 0 : 4;
  }
  if (argc >= 3 && std::string(argv[1]) == "--dsl") {
    for (int i = 2; i < argc; ++i) {
      if (std::string(argv[i]).empty() || std::string(argv[i]).rfind("--", 0) == 0) {
        std::cerr << usage;
        return 2;
      }
    }
    const std::string schema(reinterpret_cast<const char *>(lmbetterp::embedded_schema),
                             lmbetterp::embedded_schema_size);
    dslvm::Context context;
    context.set("ts", &host);
    std::vector<dslvm::Program> programs;
    for (int i = 2; i < argc; ++i) {
      try {
        programs.push_back(dslvm::Program::load(dslvm::read_file(argv[i]), schema, reg));
      } catch (const dslvm::Error &error) {
        return dslvm::report_error("load", error);
      }
    }
    for (const auto &program : programs) {
      try {
        program.run(context, reg);
      } catch (const dslvm::Error &error) {
        return dslvm::report_error("run", error);
      }
      dslvm::report_ok();
    }
    return 0;
  }
  std::cerr << usage;
  return 2;
}
