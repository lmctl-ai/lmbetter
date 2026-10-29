#pragma once
// Code-shaped (semgrep-style) patterns compiled to tree-sitter queries — roadmap wave 2 item 6.
//
//   strcpy($DST, $SRC)      -> ((call_expression function: (identifier) @lit_1 arguments: (argument_list
//                               . (_) @mv_DST_1 . (_) @mv_SRC_1 .)) @root (#eq? @lit_1 "strcpy"))
//
// The pattern is parsed as a code fragment with the SAME grammar as the code it will match (C: a
// translation unit, retried with a trailing ';' so `strcpy(a, b)` is an expression; Python: a module),
// then its CST is turned into a query: node kinds and field names give the shape, leaves (identifiers,
// literals, strings) match by text (`#eq?`), `$NAME` matches any one named node (`(_)`) and equal
// spellings of a metavariable must have equal text, and `...` matches zero or more call arguments at
// its position. Whitespace and comments in the pattern are irrelevant. First slice: one statement or
// expression per pattern, `...` only inside a call's argument list; no `pattern-either`, no `...` for
// statements. See durable-memory/11-pattern-rules.md for the corner cases.

#include "host.hpp"

#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace lmok {

// A malformed pattern: what() names the fragment and the reason (used as a pack error, exit 2).
struct PatternError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

struct CompiledPattern {
  std::string query;                                  // the generated S-expression (one pattern, root captured as @root)
  std::vector<std::string> metavariables;             // distinct `$NAME`s in order of first appearance
  std::map<std::string, std::string> first_capture;   // NAME -> capture name of its first occurrence (mv_NAME_1)
};

// Compile `pattern` for `grammar`. Throws PatternError.
CompiledPattern compile_pattern(Grammar grammar, const std::string &pattern);

// Match a compiled pattern below `root`: the @root node of every match (cursor order). The `capture`
// argument selects a metavariable's node instead of the root ("" = root); it must be a key of
// first_capture. Throws QueryError only for an internal query failure.
NodeList pattern_matches(const Node &root, const CompiledPattern &compiled, Grammar grammar,
                         const std::string &capture = "");

} // namespace lmok
