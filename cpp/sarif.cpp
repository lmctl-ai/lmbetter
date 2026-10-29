// SARIF 2.1.0 emission; see sarif.hpp for the contract.
#include "sarif.hpp"

#include <sstream>

namespace lmok {

std::string sarif_log(const std::string &tool_version,
                      const std::vector<SarifRuleMeta> &rules,
                      const std::vector<SarifResult> &results,
                      const std::function<std::string(const std::string &)> &escape) {
  std::ostringstream out;
  out << "{\"version\":\"2.1.0\",\"$schema\":\"https://json.schemastore.org/sarif-2.1.0.json\""
      << ",\"runs\":[{\"tool\":{\"driver\":{\"name\":\"lmok\",\"version\":\""
      << escape(tool_version) << "\",\"rules\":[";
  bool first = true;
  for (const auto &rule : rules) {
    if (!first) out << ",";
    first = false;
    out << "{\"id\":\"" << escape(rule.id) << "\"";
    if (!rule.description.empty())
      out << ",\"shortDescription\":{\"text\":\"" << escape(rule.description) << "\"}";
    if (!rule.help_uri.empty())
      out << ",\"helpUri\":\"" << escape(rule.help_uri) << "\"";
    out << "}";
  }
  out << "]}},\"results\":[";
  first = true;
  for (const auto &f : results) {
    if (!first) out << ",";
    first = false;
    // The example's mapping: critical -> error, minor -> note, else warning.
    const char *level = f.severity == "critical" ? "error" : f.severity == "minor" ? "note" : "warning";
    out << "{\"ruleId\":\"" << escape(f.rule) << "\",\"level\":\"" << level << "\""
        << ",\"message\":{\"text\":\"" << escape(f.message) << "\"}";
    if (!f.uri.empty())
      out << ",\"locations\":[{\"physicalLocation\":{\"artifactLocation\":{\"uri\":\""
          << escape(f.uri) << "\"},\"region\":{\"startLine\":" << f.row + 1
          << ",\"startColumn\":" << f.col + 1
          << ",\"endLine\":" << f.end_row + 1
          << ",\"endColumn\":" << f.end_col + 1 << "}}}]";
    out << "}";
  }
  out << "]}]}\n";
  return out.str();
}

} // namespace lmok
