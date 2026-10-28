// lmbetter wrapper over tree-sitter. The only translation unit that includes
// tree-sitter headers; see host.hpp for the interface contract.
#include "host.hpp"

#include <algorithm>
#include <cstdlib>
#include <new>
#include <tree_sitter/api.h>

extern "C" const TSLanguage *tree_sitter_json(void);

namespace lmbetter {

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

Tree parse_json(const std::string &text) {
  TSParser *parser = ts_parser_new();
  ts_parser_set_language(parser, tree_sitter_json());
  Tree result;
  result.tree_ = ts_parser_parse_string(parser, nullptr, text.data(),
                                        static_cast<uint32_t>(text.size()));
  result.source_ = text;
  ts_parser_delete(parser);
  return result;
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

NodeList node_query(const Node &root_node, const std::string &pattern) {
  uint32_t error_offset = 0;
  TSQueryError error_type = TSQueryErrorNone;
  TSQuery *query = ts_query_new(tree_sitter_json(), pattern.data(),
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
