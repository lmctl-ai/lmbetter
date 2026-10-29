// SARIF 2.1.0 emission; see sarif.hpp for the contract.
#include "sarif.hpp"

#include <algorithm>
#include <sstream>

namespace lmok {

namespace {

std::string uri_encode_path(const std::string &path) {
  std::ostringstream out;
  static constexpr char hex[] = "0123456789ABCDEF";
  const bool windows_drive = path.size() >= 3 &&
      ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
      path[1] == ':' && (path[2] == '/' || path[2] == '\\');
  if (windows_drive) out << "file:///" << path[0] << ':';
  const size_t begin = windows_drive ? 2 : 0;
  for (size_t i = begin; i < path.size(); ++i) {
    unsigned char c = static_cast<unsigned char>(path[i]);
    if (windows_drive && c == '\\') c = '/';
    const bool safe = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                      (c >= '0' && c <= '9') || c == '-' || c == '_' ||
                      c == '.' || c == '~' || c == '/';
    if (safe) out << static_cast<char>(c);
    else out << '%' << hex[c >> 4] << hex[c & 15];
  }
  return out.str();
}

size_t line_start(const std::string &source, int64_t row) {
  if (row <= 0) return 0;
  size_t at = 0;
  for (int64_t n = 0; n < row; ++n) {
    at = source.find('\n', at);
    if (at == std::string::npos) return source.size();
    ++at;
  }
  return at;
}

// Tree-sitter columns are UTF-8 byte offsets. SARIF's unicodeCodePoints
// columns count Unicode scalar values, so malformed bytes count individually.
int64_t codepoint_column(const std::string &source, int64_t row, int64_t byte_col) {
  if (byte_col <= 0) return 0;
  const size_t start = line_start(source, row);
  const size_t line_end = source.find('\n', start);
  const size_t limit = std::min(start + static_cast<size_t>(byte_col),
                                line_end == std::string::npos ? source.size() : line_end);
  int64_t count = 0;
  for (size_t i = start; i < limit; ++count) {
    const unsigned char c = static_cast<unsigned char>(source[i]);
    size_t width = 1;
    if (c >= 0xC2 && c <= 0xDF && i + 1 < limit &&
        (static_cast<unsigned char>(source[i + 1]) & 0xC0) == 0x80) width = 2;
    else if (c >= 0xE0 && c <= 0xEF && i + 2 < limit &&
             (static_cast<unsigned char>(source[i + 1]) & 0xC0) == 0x80 &&
             (static_cast<unsigned char>(source[i + 2]) & 0xC0) == 0x80) width = 3;
    else if (c >= 0xF0 && c <= 0xF4 && i + 3 < limit &&
             (static_cast<unsigned char>(source[i + 1]) & 0xC0) == 0x80 &&
             (static_cast<unsigned char>(source[i + 2]) & 0xC0) == 0x80 &&
             (static_cast<unsigned char>(source[i + 3]) & 0xC0) == 0x80) width = 4;
    i += width;
  }
  return count;
}

} // namespace

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
    std::string help_uri=rule.help_uri;
    const auto url=rule.metadata.strings.find("url");
    if(help_uri.empty()&&url!=rule.metadata.strings.end())help_uri=url->second;
    if (!help_uri.empty())
      out << ",\"helpUri\":\"" << escape(help_uri) << "\"";
    if(!rule.metadata.empty())out<<",\"properties\":"<<rule.metadata.json();
    out << "}";
  }
  // SARIF defines columnKind on the run. lmok normalizes source positions to
  // Unicode code points before emitting locations.
  out << "]}},\"columnKind\":\"unicodeCodePoints\",\"results\":[";
  first = true;
  for (const auto &f : results) {
    if (!first) out << ",";
    first = false;
    // The example's mapping: critical -> error, minor -> note, else warning.
    const char *level = f.severity == "critical" ? "error" : f.severity == "minor" ? "note" : "warning";
    out << "{\"ruleId\":\"" << escape(f.rule) << "\",\"level\":\"" << level << "\""
        << ",\"message\":{\"text\":\"" << escape(f.message) << "\"}";
    if(!f.metadata.empty())out<<",\"properties\":"<<f.metadata.json();
    if (!f.uri.empty()) {
      const bool have_source = f.source && !f.source->empty();
      const int64_t start_col = have_source ? codepoint_column(*f.source, f.row, f.col) : f.col;
      const int64_t end_col = have_source ? codepoint_column(*f.source, f.end_row, f.end_col) : f.end_col;
      out << ",\"locations\":[{\"physicalLocation\":{\"artifactLocation\":{\"uri\":\""
          << escape(uri_encode_path(f.uri)) << "\"},\"region\":{\"startLine\":" << f.row + 1
          << ",\"startColumn\":" << start_col + 1
          << ",\"endLine\":" << f.end_row + 1
          << ",\"endColumn\":" << end_col + 1 << "}}}]";
    }
    out << "}";
  }
  out << "]}]}\n";
  return out.str();
}

} // namespace lmok
