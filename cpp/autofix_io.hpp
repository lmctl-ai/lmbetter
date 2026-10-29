#pragma once
// Binary file I/O and a dependency-free unified preview. Verification happens
// before this layer: neither a rejected batch nor a preview opens the target
// for writing. Previews omit all source and replacement text: a fix pack
// cannot establish that unrelated or suppressed source contains no credentials.
#include "input.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace lmok {
inline std::vector<std::string> fix_lines(const std::string &text) {
  std::vector<std::string> lines;
  for (size_t start = 0; start < text.size();) {
    const size_t end = text.find('\n', start);
    const size_t next = end == std::string::npos ? text.size() : end + 1;
    lines.push_back(text.substr(start, next - start));
    start = next;
  }
  return lines;
}
inline std::string unified_fix_diff(const std::string &path, const std::string &before,
                                    const std::string &after) {
  if (before == after) return {};
  const auto old_lines = fix_lines(before), new_lines = fix_lines(after);
  std::ostringstream out;
  // A tab terminates the pathname even when it contains spaces or UTF-8.
  out << "--- " << path << "\t\n+++ " << path << "\t\n@@ -"
      << (old_lines.empty() ? 0 : 1) << "," << old_lines.size() << " +"
      << (new_lines.empty() ? 0 : 1) << "," << new_lines.size() << " @@\n";
  for (const auto &side : {std::make_pair('-', &old_lines), std::make_pair('+', &new_lines)}) {
    for (const auto &line : *side.second) {
      out << side.first << "[source omitted]";
      if (!line.empty() && line.back() == '\n') out << "\n";
      if (line.empty() || line.back() != '\n') out << "\n\\ No newline at end of file\n";
    }
  }
  return out.str();
}
inline void write_fixed_file(const std::string &path, const std::string &before,
                             const std::string &after) {
  // Avoid overwriting a concurrent modification made while rules ran.
  if (read_input_file(path) != before) throw dslvm::Error("autofix refused: file changed during lint: " + path);
  std::ofstream out(std::filesystem::u8path(path), std::ios::binary | std::ios::trunc);
  if (!out) throw dslvm::Error("cannot write " + path);
  out.write(after.data(), static_cast<std::streamsize>(after.size()));
  out.close();
  if (!out) {
    // Best-effort restoration on I/O failure. Parse/safety failures never get
    // here: original bytes remain untouched until the full batch is verified.
    std::ofstream restore(std::filesystem::u8path(path), std::ios::binary | std::ios::trunc);
    restore.write(before.data(), static_cast<std::streamsize>(before.size()));
    throw dslvm::Error("cannot write complete autofix: " + path);
  }
}
} // namespace lmok
