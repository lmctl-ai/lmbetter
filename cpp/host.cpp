// lmbetter wrapper over tree-sitter. The only translation unit that includes
// tree-sitter headers; see host.hpp for the interface contract.
#include "host.hpp"

#include <cstdlib>
#include <tree_sitter/api.h>

extern "C" const TSLanguage *tree_sitter_json(void);

namespace lmbetter {

const void *tree_handle(const Tree &tree) { return tree.tree_; }

Tree::~Tree() {
  if (tree_) ts_tree_delete(static_cast<TSTree *>(tree_));
}

Tree::Tree(Tree &&other) noexcept : tree_(other.tree_) { other.tree_ = nullptr; }

Tree &Tree::operator=(Tree &&other) noexcept {
  if (this != &other) {
    if (tree_) ts_tree_delete(static_cast<TSTree *>(tree_));
    tree_ = other.tree_;
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

} // namespace lmbetter
