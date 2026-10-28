// lmbetter wrapper over tree-sitter. The only translation unit that includes
// tree-sitter headers; see host.hpp for the interface contract.
#include "host.hpp"

#include <algorithm>
#include <cstdlib>
#include <new>
#include <regex>
#include <tree_sitter/api.h>

extern "C" const TSLanguage *tree_sitter_json(void);
extern "C" const TSLanguage *tree_sitter_c(void);

namespace lmbetter {

static const TSLanguage *language_of(Grammar grammar) {
  switch (grammar) {
  case Grammar::Json: return tree_sitter_json();
  case Grammar::C: return tree_sitter_c();
  }
  return tree_sitter_json();
}

static_assert(sizeof(TSNode) <= sizeof(Node::bytes) && alignof(TSNode) <= alignof(void *),
              "Node::bytes must hold a TSNode");

static TSNode &ts(Node &node) { return *reinterpret_cast<TSNode *>(node.bytes); }
static const TSNode &ts(const Node &node) { return *reinterpret_cast<const TSNode *>(node.bytes); }

const void *tree_handle(const Tree &tree) { return tree.tree_; }

Tree::~Tree() {
  if (tree_) ts_tree_delete(static_cast<TSTree *>(tree_));
}

Tree::Tree(Tree &&other) noexcept : tree_(other.tree_), source_(std::move(other.source_)) {
  other.tree_ = nullptr;
}

Tree &Tree::operator=(Tree &&other) noexcept {
  if (this != &other) {
    if (tree_) ts_tree_delete(static_cast<TSTree *>(tree_));
    tree_ = other.tree_;
    source_ = std::move(other.source_);
    other.tree_ = nullptr;
  }
  return *this;
}

Tree parse(Grammar grammar, const std::string &text) {
  TSParser *parser = ts_parser_new();
  ts_parser_set_language(parser, language_of(grammar));
  Tree result;
  result.tree_ = ts_parser_parse_string(parser, nullptr, text.data(),
                                        static_cast<uint32_t>(text.size()));
  result.source_ = text;
  ts_parser_delete(parser);
  return result;
}

static TSPoint point_for_byte(const std::string &source, uint32_t byte) {
  TSPoint point = {0, 0};
  const uint32_t limit = std::min<uint32_t>(byte, source.size());
  for (uint32_t i = 0; i < limit; ++i) {
    if (source[i] == '\n') { ++point.row; point.column = 0; }
    else ++point.column;
  }
  return point;
}

void tree_edit(Tree &tree, int64_t start_byte, int64_t old_end_byte, int64_t new_end_byte) {
  if (tree.has_pending_edit_)
    throw std::out_of_range("Tree.edit: one pending edit per reparse");
  const int64_t size = static_cast<int64_t>(tree.source_.size());
  if (start_byte < 0 || old_end_byte < start_byte || old_end_byte > size || new_end_byte < start_byte)
    throw std::out_of_range("Tree.edit: bad byte offsets (need 0 <= start <= old_end <= " +
                            std::to_string(size) + " and new_end >= start)");
  tree.has_pending_edit_ = true;
  tree.edit_start_ = start_byte;
  tree.edit_old_end_ = old_end_byte;
  tree.edit_new_end_ = new_end_byte;
}

Tree reparse(Grammar grammar, Tree &old_tree, const std::string &new_text) {
  if (old_tree.has_pending_edit_) {
    TSInputEdit edit;
    edit.start_byte = static_cast<uint32_t>(old_tree.edit_start_);
    edit.old_end_byte = static_cast<uint32_t>(old_tree.edit_old_end_);
    edit.new_end_byte = static_cast<uint32_t>(old_tree.edit_new_end_);
    edit.start_point = point_for_byte(old_tree.source_, edit.start_byte);
    edit.old_end_point = point_for_byte(old_tree.source_, edit.old_end_byte);
    edit.new_end_point = point_for_byte(new_text, edit.new_end_byte);
    ts_tree_edit(static_cast<TSTree *>(old_tree.tree_), &edit);
    old_tree.has_pending_edit_ = false;
  }
  TSParser *parser = ts_parser_new();
  ts_parser_set_language(parser, language_of(grammar));
  Tree result;
  result.tree_ = ts_parser_parse_string(parser, static_cast<TSTree *>(old_tree.tree_),
                                        new_text.data(), static_cast<uint32_t>(new_text.size()));
  result.source_ = new_text;
  ts_parser_delete(parser);
  return result;
}

std::string changed_ranges_json(const Tree &old_tree, const Tree &new_tree) {
  uint32_t count = 0;
  TSRange *ranges = ts_tree_get_changed_ranges(
      static_cast<const TSTree *>(old_tree.tree_), static_cast<const TSTree *>(new_tree.tree_), &count);
  std::string out = "[";
  for (uint32_t i = 0; i < count; ++i) {
    if (i) out += ",";
    out += "[" + std::to_string(ranges[i].start_byte) + "," + std::to_string(ranges[i].end_byte) + "]";
  }
  std::free(ranges);
  return out + "]";
}

static TSNode root(const Tree &tree) {
  return ts_tree_root_node(static_cast<TSTree *>(const_cast<void *>(tree_handle(tree))));
}

std::string tree_sexp(const Tree &tree) {
  char *text = ts_node_string(root(tree));
  std::string out = text ? text : "";
  std::free(text);
  return out;
}

bool tree_has_error(const Tree &tree) { return ts_node_has_error(root(tree)); }

int64_t tree_node_count(const Tree &tree) {
  return static_cast<int64_t>(ts_node_descendant_count(root(tree))) + 1;
}

Node node_root(const Tree &tree) {
  Node node;
  node.owner = &tree;
  new (node.bytes) TSNode(root(tree));
  return node;
}

NodeList tree_nodes(const Tree &tree) {
  NodeList list;
  std::vector<TSNode> stack;
  stack.push_back(root(tree));
  while (!stack.empty()) {
    const TSNode current = stack.back();
    stack.pop_back();
    Node node;
    node.owner = &tree;
    new (node.bytes) TSNode(current);
    list.items.push_back(node);
    const uint32_t count = ts_node_named_child_count(current);
    for (uint32_t i = count; i > 0; --i)
      stack.push_back(ts_node_named_child(current, i - 1));
  }
  return list;
}

std::string node_kind(const Node &node) { return ts_node_type(ts(node)); }

std::string node_text(const Node &node) {
  const std::string &source = node.owner->source_;
  const uint32_t start = ts_node_start_byte(ts(node));
  const uint32_t end = ts_node_end_byte(ts(node));
  if (end <= start || start >= source.size()) return "";
  return source.substr(start, std::min<uint32_t>(end, source.size()) - start);
}

bool node_is_named(const Node &node) { return ts_node_is_named(ts(node)); }

int64_t node_start_row(const Node &node) { return ts_node_start_point(ts(node)).row; }
int64_t node_start_col(const Node &node) { return ts_node_start_point(ts(node)).column; }
int64_t node_end_row(const Node &node) { return ts_node_end_point(ts(node)).row; }
int64_t node_end_col(const Node &node) { return ts_node_end_point(ts(node)).column; }

int64_t node_child_count(const Node &node) {
  return static_cast<int64_t>(ts_node_named_child_count(ts(node)));
}

Node node_child(const Node &node, int64_t index) {
  Node child;
  child.owner = node.owner;
  new (child.bytes) TSNode(ts_node_named_child(ts(node), static_cast<uint32_t>(index)));
  return child;
}

bool node_has_parent(const Node &node) {
  return !ts_node_is_null(ts_node_parent(ts(node)));
}

Node node_parent(const Node &node) {
  Node parent;
  parent.owner = node.owner;
  new (parent.bytes) TSNode(ts_node_parent(ts(node)));
  return parent;
}

static const char *query_error_word(TSQueryError error) {
  switch (error) {
  case TSQueryErrorSyntax: return "syntax";
  case TSQueryErrorNodeType: return "node-type";
  case TSQueryErrorField: return "field";
  case TSQueryErrorCapture: return "capture";
  case TSQueryErrorStructure: return "structure";
  case TSQueryErrorLanguage: return "language";
  default: return "unknown";
  }
}

// ---- query predicates (E1) -------------------------------------------------
// Evaluated per match, following the upstream tree-sitter semantics (see
// lib/binding_rust/lib.rs satisfies_text_predicates): #eq?/#not-eq? compare
// capture node TEXT (source slice) against a literal or another capture;
// #match?/#not-match? apply an UNANCHORED ECMAScript regex to the text
// (upstream uses regex::is_match; authors anchor with ^...$ themselves).
// With quantified captures the predicate must hold for ALL nodes under the
// capture (upstream's match_all behavior for the non-any variants). Unknown
// operators (#set!, #is?, #any-of?, ...) are directives/unsupported
// predicates and are IGNORED, like the tree-sitter CLI.
static Node wrap_node(const Tree *owner, TSNode raw) {
  Node node;
  node.owner = owner;
  new (node.bytes) TSNode(raw);
  return node;
}

static std::vector<std::string> capture_texts(const TSQueryMatch &match, uint32_t capture_index,
                                              const Tree *owner) {
  std::vector<std::string> texts;
  for (uint16_t c = 0; c < match.capture_count; ++c)
    if (match.captures[c].index == capture_index)
      texts.push_back(node_text(wrap_node(owner, match.captures[c].node)));
  return texts;
}

static bool predicates_ok(const TSQuery *query, const TSQueryMatch &match, const Tree *owner) {
  uint32_t step_count = 0;
  const TSQueryPredicateStep *steps =
      ts_query_predicates_for_pattern(query, match.pattern_index, &step_count);
  if (!steps) return true;
  uint32_t i = 0;
  while (i < step_count) {
    if (steps[i].type != TSQueryPredicateStepTypeString) { // malformed; skip to Done
      while (i < step_count && steps[i].type != TSQueryPredicateStepTypeDone) ++i;
      ++i;
      continue;
    }
    uint32_t len = 0;
    const char *op = ts_query_string_value_for_id(query, steps[i].value_id, &len);
    const std::string opname(op, len);
    ++i;
    std::vector<std::pair<bool, uint32_t>> args; // (is_capture, value_id)
    while (i < step_count && steps[i].type != TSQueryPredicateStepTypeDone) {
      args.push_back({steps[i].type == TSQueryPredicateStepTypeCapture, steps[i].value_id});
      ++i;
    }
    ++i; // past Done
    if (opname != "eq?" && opname != "not-eq?" && opname != "match?" && opname != "not-match?")
      continue; // unknown predicate/directive: ignore
    if (args.size() != 2 || !args[0].first) continue; // malformed: ignore
    const std::vector<std::string> texts = capture_texts(match, args[0].second, owner);
    const bool positive = opname == "eq?" || opname == "match?";
    bool all_ok = true;
    if (args[1].first) { // capture-vs-capture (eq? family)
      const std::vector<std::string> other = capture_texts(match, args[1].second, owner);
      for (const auto &t : texts)
        for (const auto &o : other)
          if ((t == o) != positive) { all_ok = false; break; }
    } else {
      uint32_t slen = 0;
      const char *sv = ts_query_string_value_for_id(query, args[1].second, &slen);
      const std::string operand(sv, slen);
      if (opname == "match?" || opname == "not-match?") {
        std::regex re;
        try { re = std::regex(operand); }
        catch (const std::regex_error &) {
          throw QueryError("invalid #match? regex '" + operand + "'");
        }
        for (const auto &t : texts)
          if (static_cast<bool>(std::regex_search(t, re)) != positive) { all_ok = false; break; }
      } else {
        for (const auto &t : texts)
          if ((t == operand) != positive) { all_ok = false; break; }
      }
    }
    if (!all_ok) return false;
  }
  return true;
}

NodeList node_query(const Node &root_node, const std::string &pattern, Grammar grammar) {
  uint32_t error_offset = 0;
  TSQueryError error_type = TSQueryErrorNone;
  TSQuery *query = ts_query_new(language_of(grammar), pattern.data(),
                                static_cast<uint32_t>(pattern.size()),
                                &error_offset, &error_type);
  if (!query)
    throw QueryError("query error at byte offset " + std::to_string(error_offset) +
                     ": " + query_error_word(error_type));
  TSQueryCursor *cursor = ts_query_cursor_new();
  ts_query_cursor_exec(cursor, query, ts(root_node));
  NodeList list;
  TSQueryMatch match;
  while (ts_query_cursor_next_match(cursor, &match)) {
    if (!predicates_ok(query, match, root_node.owner)) continue;
    for (uint16_t i = 0; i < match.capture_count; ++i) {
      Node node;
      node.owner = root_node.owner;
      new (node.bytes) TSNode(match.captures[i].node);
      list.items.push_back(node);
    }
  }
  ts_query_cursor_delete(cursor);
  ts_query_delete(query);
  return list;
}

} // namespace lmbetter
