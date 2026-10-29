#pragma once

// Conservative, token-safe source edits used by the lint host.  This file is
// deliberately independent of tree-sitter: all parser access goes through
// host.hpp so the edit verifier remains portable and testable.

#include "fix.hpp"

#include <algorithm>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace lmok {

namespace autofix_detail {

inline bool utf8_boundary(const std::string &s, int64_t at) {
  if (at < 0 || static_cast<size_t>(at) > s.size()) return false;
  if (at == 0 || static_cast<size_t>(at) == s.size()) return true;
  const unsigned char c = static_cast<unsigned char>(s[static_cast<size_t>(at)]);
  return (c & 0xc0) != 0x80;
}

inline std::string crlf_text(const std::string &old, const std::string &replacement) {
  // A CRLF input keeps its line-ending convention when a fix contains lines.
  bool has_crlf = false;
  for (size_t i = 1; i < old.size(); ++i)
    if (old[i - 1] == '\r' && old[i] == '\n') { has_crlf = true; break; }
  if (!has_crlf) return replacement;
  std::string out;
  out.reserve(replacement.size() + 8);
  for (size_t i = 0; i < replacement.size(); ++i) {
    if (replacement[i] == '\n' && (i == 0 || replacement[i - 1] != '\r')) out += '\r';
    out += replacement[i];
  }
  return out;
}

struct Span {
  int64_t start = 0, end = 0;
  bool operator<(const Span &other) const {
    return start < other.start || (start == other.start && end < other.end);
  }
};

inline bool overlaps(const Span &a, const Span &b) {
  return a.start < b.end && b.start < a.end;
}

inline int64_t shift(int64_t byte, const std::vector<FixEdit> &edits) {
  int64_t delta = 0;
  for (const auto &edit : edits) {
    if (edit.end_byte <= byte) delta += static_cast<int64_t>(edit.text.size()) - (edit.end_byte - edit.start_byte);
  }
  return byte + delta;
}

struct ShapeEntry {
  int depth = 0;
  std::string kind, text;
  bool named = false;
  bool operator==(const ShapeEntry &other) const {
    return depth == other.depth && named == other.named && kind == other.kind && text == other.text;
  }
};

inline bool intersects(int64_t start, int64_t end, const FixEdit &edit) {
  return start < edit.end_byte && edit.start_byte < end;
}

// Keep the complete CST topology while replacing each edited subtree with a
// marker. Ancestor nodes therefore remain in the signature: a replacement
// cannot silently merge siblings or change the containing construct.
inline std::vector<ShapeEntry> masked_shape(const Tree &tree, const std::vector<FixEdit> &edits) {
  std::vector<ShapeEntry> result;
  int skipped_depth = -1;
  walk_nodes(tree, [&](const Node &node, const std::vector<Node> &ancestors) {
    const int depth = static_cast<int>(ancestors.size());
    if (skipped_depth >= 0 && depth > skipped_depth) return;
    skipped_depth = -1;
    const int64_t start = node_start_byte(node), end = node_end_byte(node);
    for (const auto &edit : edits) {
      if (start == edit.start_byte && end == edit.end_byte) {
        result.push_back({depth, "<autofix-replaced-node>", "", true});
        skipped_depth = depth;
        return;
      }
    }
    std::string text;
    bool affected = false;
    for (const auto &edit : edits) if (intersects(start, end, edit)) { affected = true; break; }
    // Text of ancestors containing an edit necessarily changes. Their kind,
    // name and depth still participate in the structural comparison.
    if (!affected && start != end && node_child_count(node) == 0) text = node_text(node);
    result.push_back({depth, node_kind(node), std::move(text), node_is_named(node)});
  });
  return result;
}

inline Tree verify_tree(const std::string &old_text, const std::string &new_text,
                        const std::vector<FixEdit> &edits, Grammar grammar) {
  Tree old_tree = parse(grammar, old_text);
  if (tree_has_error(old_tree)) throw std::runtime_error("autofix refused: source contains ERROR nodes");
  Tree new_tree = parse(grammar, new_text);
  if (tree_has_error(new_tree)) throw std::runtime_error("autofix refused: replacement introduces ERROR nodes");
  std::vector<FixEdit> mapped;
  mapped.reserve(edits.size());
  for (const auto &edit : edits) {
    FixEdit copy = edit;
    copy.start_byte = shift(edit.start_byte, edits);
    copy.end_byte = copy.start_byte + static_cast<int64_t>(edit.text.size());
    mapped.push_back(std::move(copy));
  }
  // Each replacement must correspond to one new CST node. This rejects a
  // token splice that changes the node boundary before comparing the masked
  // full-tree signatures.
  const auto new_shapes = masked_shape(new_tree, mapped);
  for (const auto &edit : mapped) {
    size_t found = 0;
    walk_nodes(new_tree, [&](const Node &node, const std::vector<Node> &) {
      if (node_start_byte(node) == edit.start_byte && node_end_byte(node) == edit.end_byte) ++found;
    });
    if (found != 1) throw std::runtime_error("autofix refused: replacement is not one unambiguous syntax node");
  }
  const auto old_shapes = masked_shape(old_tree, edits);
  if (old_shapes.size() != new_shapes.size())
    throw std::runtime_error("autofix refused: replacement changes tree shape outside the replaced node");
  for (size_t i = 0; i < old_shapes.size(); ++i) {
    if (!(old_shapes[i] == new_shapes[i]))
      throw std::runtime_error("autofix refused: replacement changes tree shape outside the replaced node");
  }
  return new_tree;
}

} // namespace autofix_detail

inline FixResult apply_fixes(const std::string &source, const std::vector<FixEdit> &edits) {
  if (edits.empty()) return {source, 0, {}};
  std::vector<FixEdit> ordered;
  ordered.reserve(edits.size());
  for (auto edit : edits) {
    edit.text = autofix_detail::crlf_text(source, edit.text);
    if (edit.start_byte >= 0 && edit.end_byte >= edit.start_byte &&
        static_cast<size_t>(edit.end_byte) <= source.size() &&
        source.compare(static_cast<size_t>(edit.start_byte),
                       static_cast<size_t>(edit.end_byte - edit.start_byte), edit.text) == 0)
      continue; // An already-applied edit is idempotent and does not count as fixed.
    ordered.push_back(edit);
  }
  if (ordered.empty()) return {source, 0, {}};
  std::sort(ordered.begin(), ordered.end(), [](const FixEdit &a, const FixEdit &b) {
    return a.start_byte < b.start_byte || (a.start_byte == b.start_byte && a.end_byte < b.end_byte);
  });
  for (size_t i = 0; i < ordered.size(); ++i) {
    const auto &edit = ordered[i];
    if (edit.start_byte < 0 || edit.end_byte <= edit.start_byte ||
        static_cast<size_t>(edit.end_byte) > source.size() ||
        !autofix_detail::utf8_boundary(source, edit.start_byte) ||
        !autofix_detail::utf8_boundary(source, edit.end_byte))
      throw std::runtime_error("autofix refused: invalid or non-token-aligned byte range");
    if (i && autofix_detail::overlaps({ordered[i - 1].start_byte, ordered[i - 1].end_byte},
                                      {edit.start_byte, edit.end_byte}))
      throw std::runtime_error("autofix refused: overlapping fixes");
  }

  std::map<Grammar, std::vector<FixEdit>> by_grammar;
  for (const auto &edit : ordered) by_grammar[edit.grammar].push_back(edit);
  if (by_grammar.size() > 1)
    throw std::runtime_error("autofix refused: fixes from multiple grammars cannot share one file");
  for (const auto &[grammar, grammar_edits] : by_grammar) {
    Tree tree = parse(grammar, source);
    if (tree_has_error(tree)) throw std::runtime_error("autofix refused: source contains ERROR nodes");
    std::vector<autofix_detail::Span> nodes;
    walk_nodes(tree, [&](const Node &node, const std::vector<Node> &) {
      if (node_start_byte(node) != node_end_byte(node))
        nodes.push_back({node_start_byte(node), node_end_byte(node)});
    });
    for (const auto &edit : grammar_edits) {
      size_t exact = 0;
      for (const auto &node : nodes) if (node.start == edit.start_byte && node.end == edit.end_byte) {
        if (node.start == 0 && node.end == static_cast<int64_t>(source.size()))
          throw std::runtime_error("autofix refused: whole-file replacement is not token-safe");
        ++exact;
      }
      // Equal-range wrappers (Bash command/command_name/word, for example)
      // cannot be distinguished by a byte-only fix. Masking their outermost
      // node would conceal ancestor changes, so refuse ambiguous ranges.
      if (exact != 1) throw std::runtime_error("autofix refused: fix range is not one unambiguous syntax node");
    }
  }

  std::string result = source;
  for (auto it = ordered.rbegin(); it != ordered.rend(); ++it) {
    result.replace(static_cast<size_t>(it->start_byte), static_cast<size_t>(it->end_byte - it->start_byte), it->text);
  }
  std::map<Grammar, Tree> verified_trees;
  for (const auto &[grammar, grammar_edits] : by_grammar)
    verified_trees.emplace(grammar, autofix_detail::verify_tree(source, result, grammar_edits, grammar));
  return {std::move(result), ordered.size(), std::move(verified_trees)};
}

} // namespace lmok
