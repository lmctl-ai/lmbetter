#pragma once
// SARIF 2.1.0 emission behind `lmok lint|check --sarif FILE`. This is the
// examples/sarif-report.lms mapping ported into the host, byte-for-byte in
// structure: version 2.1.0 with the schemastore $schema, one run whose
// tool.driver is "lmok", results carry ruleId/level/message and a
// physicalLocation whose region converts the host's 0-based positions to
// SARIF's 1-based startLine/startColumn (endColumn 1-based exclusive,
// matching the 0-based exclusive end_col after +1). The severity -> level
// mapping is the example's: critical -> error, minor -> note, everything
// else (the free-form vocabulary is pack-owned) -> warning.

#include "rule_metadata.hpp"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace lmok {

struct SarifRuleMeta {
  std::string id;
  std::string description; // empty = omitted (shortDescription.text)
  std::string help_uri;    // empty = omitted (helpUri)
  RuleMetadata metadata{}; // Always included in explicit SARIF, independently of --metadata JSON.
};

struct SarifResult {
  std::string rule, severity, message;
  std::string uri; // path exactly as lint/check printed it; empty = unlocated
                   // (check "body" findings: no locations member is emitted)
  int64_t row = 0, col = 0, end_row = 0, end_col = 0; // 0-based
  RuleMetadata metadata{};
};

// `escape` is the host JSON string escaper WITHOUT surrounding quotes (the
// one used for finding output, so malformed source bytes become  in
// SARIF exactly as in the JSON-lines output).
std::string sarif_log(const std::string &tool_version,
                      const std::vector<SarifRuleMeta> &rules,
                      const std::vector<SarifResult> &results,
                      const std::function<std::string(const std::string &)> &escape);

} // namespace lmok
