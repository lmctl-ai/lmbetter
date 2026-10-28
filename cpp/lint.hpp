#pragma once
// Rule-pack loading and the lint dispatcher behind `lmbetter lint`.
//
// A rule pack is conditions-as-data: each rule declares an `on` node kind
// (mirrors the @annotation value — the v6 bytecode entry table will plug
// annotated lmcode rule fns into this same kind-indexed dispatch) or a
// tree-sitter `query` pattern, plus optional text_matches regex and the
// finding message/severity. See README "Rule packs" for the format.

#include "host.hpp"

#include <regex>
#include <string>
#include <vector>

namespace lmbetter {

struct LintRule {
  std::string id;
  Grammar grammar = Grammar::Json;
  std::string on;          // CST node kind; empty = not set
  std::string query;       // S-expression pattern; empty = not set
  std::string message;     // "{text}" expands to the matched node's text
  std::string severity = "minor"; // free-form; the pack owns the vocabulary
  std::string text_matches;      // ECMAScript regex source; empty = not set
  std::regex text_regex;         // valid only when text_matches is non-empty
};

struct LintFinding {
  int64_t row = 0, col = 0, end_row = 0, end_col = 0; // 0-based
  std::string rule, severity, message;
};

// Parse and validate a rule pack file. Throws dslvm::Error (plain message,
// naming the rule id where one exists) for malformed JSON, missing fields,
// unknown languages, bad regexes, and on/query violations.
std::vector<LintRule> load_rule_pack(const std::string &path);

// Run the dispatcher over one file's text. Rules are grouped by grammar and
// the text is parsed once per grammar used. `on` rules dispatch through ONE
// pre-order CST walk with a kind→rules index (the dispatcher core v6
// reuses); `query` rules run one ts_query cursor per rule over the root
// (merging them into the single walk is a later optimization — deliberately
// no Rete). Findings are sorted by (row, col). A query compile failure
// throws QueryError with the rule id prepended.
std::vector<LintFinding> lint_text(const std::vector<LintRule> &rules, const std::string &text);

} // namespace lmbetter
