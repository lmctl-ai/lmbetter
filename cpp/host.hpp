#pragma once
// lmbetter wrapper over the tree-sitter incremental parsing engine.
//
// This is the only lmbetter-authored interface to tree-sitter: all
// tree_sitter/api.h fallout stays in host.cpp; the CLI host (main.cpp) talks
// to this plain interface only. Parsed trees and nodes are held as opaque
// values so no tree-sitter header leaks into this file.

#include <cstdint>
#include <string>
#include <stdexcept>
#include <vector>

namespace lmbetter {

struct Node;

// The vendored grammars. The DSL surface is explicit per grammar
// (ts.parse / ts.parse_c, ts.query / ts.query_c); a third grammar should
// trigger generalizing this to a language parameter.
enum class Grammar { Json, C };

// Owning handle for one parsed tree plus a copy of its source text (kept
// alive so Node text() stays valid). Never throws: the tree-sitter parser
// always produces a tree, even for malformed input; use tree_has_error to
// detect error nodes.
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
  friend std::string node_text(const Node &node);
  friend Tree parse(Grammar grammar, const std::string &text);
  friend std::string tree_sexp(const Tree &tree);
  friend bool tree_has_error(const Tree &tree);
  friend int64_t tree_node_count(const Tree &tree);
  friend struct Node;
  void *tree_ = nullptr; // TSTree *
  std::string source_;
};

// Borrowed view of one CST node. The owning Tree must outlive the handle (it
// does: trees live in the host arena for the whole run). `bytes` holds a
// TSNode by value; host.cpp static-asserts the fit.
struct Node {
  const Tree *owner = nullptr;
  alignas(void *) unsigned char bytes[32];
};

// Parse text with the given vendored grammar (Grammar::Json is the default
// `parse` surface).
Tree parse(Grammar grammar, const std::string &text);

// The tree as a single-line s-expression, like `tree-sitter parse` prints it.
std::string tree_sexp(const Tree &tree);

// True when the tree contains an ERROR or MISSING node (malformed input).
bool tree_has_error(const Tree &tree);

// Total number of nodes in the tree, including the root.
int64_t tree_node_count(const Tree &tree);

// Root node of the tree.
Node node_root(const Tree &tree);

// Flat pre-order list of a tree's NAMED nodes, root first. Built fresh on
// every Tree.nodes() call: O(tree) time and memory, fine at this scale.
struct NodeList {
  std::vector<Node> items;
};

// Pre-order traversal of the named nodes of tree (root first).
NodeList tree_nodes(const Tree &tree);

// Thrown by node_query for an invalid pattern: what() is
// "query error at byte offset N: <syntax|node-type|field|capture|structure|language>".
struct QueryError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

// All captured nodes of all matches of a tree-sitter S-expression query
// pattern (e.g. `(pair key: (string (string_content) @key))`), executed over
// the subtree at root, in cursor order (matches in byte order, captures in
// pattern order within a match). Duplicate nodes across captures/matches are
// kept. A pattern with zero captures yields an empty list. Query predicates
// #eq?/#not-eq?/#match?/#not-match? are evaluated per match (capture text;
// #match? is UNANCHORED, matching upstream tree-sitter — authors anchor with
// ^...$); unknown predicates/directives (#set!, #is?, any-*, ...) are ignored.
NodeList node_query(const Node &root, const std::string &pattern, Grammar grammar);

// Node kind (ts_node_type), e.g. "document", "object", "pair", "string".
std::string node_kind(const Node &node);

// Source slice the node spans (includes quotes for string nodes).
std::string node_text(const Node &node);

// True for named nodes (false for anonymous tokens like punctuation).
bool node_is_named(const Node &node);

// Extent of the node. Rows and columns are 0-based (tree-sitter convention).
int64_t node_start_row(const Node &node);
int64_t node_start_col(const Node &node);
int64_t node_end_row(const Node &node);
int64_t node_end_col(const Node &node);

// Number of NAMED children (anonymous tokens are not counted).
int64_t node_child_count(const Node &node);

// The i-th NAMED child. The caller must bounds-check against
// node_child_count first; passing an out-of-range index is undefined
// (the DSL registry in main.cpp performs the check and throws).
Node node_child(const Node &node, int64_t index);

// False for the root node (ts_node_parent of the root is a null node).
bool node_has_parent(const Node &node);

// The parent node. Root is a contract violation: the DSL registry checks
// node_has_parent first and throws (exit 3); C++ callers must do the same.
Node node_parent(const Node &node);

// Opaque access to the wrapped tree (implementation detail; used only by
// host.cpp helpers). Returns nullptr for a moved-from handle.
const void *tree_handle(const Tree &tree);

} // namespace lmbetter
