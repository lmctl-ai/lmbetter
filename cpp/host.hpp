#pragma once
// lmbetterp wrapper over the tree-sitter incremental parsing engine.
//
// This is the only lmbetterp-authored interface to tree-sitter: all
// tree_sitter/api.h fallout stays in host.cpp; the CLI host (main.cpp) talks
// to this plain interface only. The parsed tree is held as an opaque pointer
// so no tree-sitter header leaks into this file.

#include <cstdint>
#include <string>

namespace lmbetterp {

// Owning handle for one parsed tree (the JSON grammar). Never throws: the
// tree-sitter parser always produces a tree, even for malformed input; use
// tree_has_error to detect error nodes.
class Tree {
public:
  Tree() = default;
  ~Tree();
  Tree(const Tree &) = delete;
  Tree &operator=(const Tree &) = delete;
  Tree(Tree &&other) noexcept;
  Tree &operator=(Tree &&other) noexcept;

private:
  friend const void *tree_handle(const Tree &tree);
  friend Tree parse_json(const std::string &text);
  friend std::string tree_sexp(const Tree &tree);
  friend bool tree_has_error(const Tree &tree);
  friend int64_t tree_node_count(const Tree &tree);
  void *tree_ = nullptr; // TSTree *
};

// Parse text as JSON with the vendored tree-sitter-json grammar.
Tree parse_json(const std::string &text);

// The tree as a single-line s-expression, like `tree-sitter parse` prints it.
std::string tree_sexp(const Tree &tree);

// True when the tree contains an ERROR or MISSING node (malformed input).
bool tree_has_error(const Tree &tree);

// Total number of nodes in the tree, including the root.
int64_t tree_node_count(const Tree &tree);

// Opaque access to the wrapped tree (implementation detail; used only by
// host.cpp helpers). Returns nullptr for a moved-from handle.
const void *tree_handle(const Tree &tree);

} // namespace lmbetterp
