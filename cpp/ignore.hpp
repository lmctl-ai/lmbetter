#pragma once

// A deliberately small, deterministic subset of gitignore.  This is not a
// gitignore implementation: there are no escaped spaces, character escapes,
// double-leading-slash special cases, or per-directory ignore files.  Rules
// are read only from <root>/.lmokignore (and from the constructor's `extra`)
// and are matched as UTF-8 path bytes.  Glob matching is dynamic programming,
// rather than std::regex, so a hostile pattern cannot trigger backtracking.

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace lmok {

class IgnoreRules {
  struct Rule {
    std::string pattern;
    bool negated = false;
    bool directory = false;
    bool anchored = false;
    bool basename = false;
  };

  std::filesystem::path root_;
  std::vector<Rule> rules_;

  static std::string path_text(const std::filesystem::path &p) {
    return p.generic_u8string();
  }

  static bool class_matches(std::string_view pattern, size_t &i, char c) {
    // i points at '['.  A closing bracket is permitted as the first member.
    const size_t start = i++;
    bool negated = false;
    if (i < pattern.size() && (pattern[i] == '!' || pattern[i] == '^')) {
      negated = true;
      ++i;
    }
    bool matched = false, member = false;
    char previous = 0;
    if (i < pattern.size() && pattern[i] == ']') {
      matched = c == ']';
      previous = ']';
      member = true;
      ++i;
    }
    while (i < pattern.size() && pattern[i] != ']') {
      const char first = pattern[i++];
      if (first == '\0') throw std::runtime_error("invalid ignore rule: NUL");
      if (i + 1 < pattern.size() && pattern[i] == '-' && pattern[i + 1] != ']') {
        i++;
        const char last = pattern[i++];
        if (first > last)
          throw std::runtime_error("invalid ignore rule: descending character range");
        matched = matched || (first <= c && c <= last);
      } else {
        matched = matched || first == c;
      }
      previous = first;
      member = true;
      (void)previous;
    }
    if (!member || i == pattern.size()) {
      (void)start;
      throw std::runtime_error("invalid ignore rule: unterminated character class");
    }
    ++i;
    return negated ? !matched : matched;
  }

  static bool glob(std::string_view pattern, std::string_view text) {
    // Two rows keep memory linear in the input, while the state space is
    // bounded by pattern length * candidate length.
    std::vector<unsigned char> row(text.size() + 1), next(text.size() + 1);
    row[0] = 1;
    for (size_t i = 0; i < pattern.size();) {
      std::fill(next.begin(), next.end(), 0);
      const char token = pattern[i];
      if (token == '*') {
        size_t end = i + 1;
        while (end < pattern.size() && pattern[end] == '*') ++end;
        const bool globstar = end - i >= 2;
        const bool slash_suffix = globstar && end < pattern.size() && pattern[end] == '/';
        if (slash_suffix) {
          // **/ is either empty (for example **/name matches name) or an
          // arbitrary prefix ending at a slash.
          bool prior = row[0];
          next[0] = row[0];
          for (size_t j = 1; j <= text.size(); ++j) {
            prior = prior || row[j];
            next[j] = static_cast<unsigned char>(row[j] || (prior && text[j - 1] == '/'));
          }
          ++end;
        } else {
          next[0] = row[0]; // zero characters
          for (size_t j = 1; j <= text.size(); ++j)
            next[j] = static_cast<unsigned char>(row[j] || (next[j - 1] && (globstar || text[j - 1] != '/')));
        }
        row.swap(next);
        i = end;
      } else if (token == '?') {
        for (size_t j = 1; j <= text.size(); ++j)
          next[j] = static_cast<unsigned char>(row[j - 1] && text[j - 1] != '/');
        row.swap(next);
        ++i;
      } else if (token == '[') {
        for (size_t j = 1; j <= text.size(); ++j) {
          size_t end = i;
          if (row[j - 1] && text[j - 1] != '/') next[j] = static_cast<unsigned char>(class_matches(pattern, end, text[j - 1]));
        }
        size_t end = i;
        (void)class_matches(pattern, end, '\0');
        row.swap(next);
        i = end;
      } else {
        for (size_t j = 1; j <= text.size(); ++j)
          next[j] = static_cast<unsigned char>(row[j - 1] && token == text[j - 1]);
        row.swap(next);
        ++i;
      }
    }
    return row[text.size()] != 0;
  }

  static Rule parse_rule(std::string line, const std::string &where) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty() || line[0] == '#') return {};
    if (line.find('\0') != std::string::npos || line.find('\\') != std::string::npos)
      throw std::runtime_error("invalid ignore rule in " + where + ": escapes are unsupported");
    Rule rule;
    if (line[0] == '!') {
      rule.negated = true;
      line.erase(0, 1);
    }
    if (line.empty()) throw std::runtime_error("invalid ignore rule in " + where + ": empty pattern");
    if (line.back() == '/') {
      rule.directory = true;
      line.pop_back();
      if (line.empty()) throw std::runtime_error("invalid ignore rule in " + where + ": root directory");
    }
    rule.anchored = line.front() == '/';
    if (rule.anchored) line.erase(0, 1);
    if (line.empty() || line.front() == '/' || line.back() == '/')
      throw std::runtime_error("invalid ignore rule in " + where + ": malformed path");
    size_t component_start = 0;
    while (component_start <= line.size()) {
      const size_t slash = line.find('/', component_start);
      const std::string component = line.substr(
          component_start, slash == std::string::npos ? line.size() - component_start
                                                        : slash - component_start);
      if (component == "..")
        throw std::runtime_error("invalid ignore rule in " + where + ": path escapes root");
      if (slash == std::string::npos) break;
      component_start = slash + 1;
    }
    if (line.find('[') != std::string::npos) {
      size_t i = 0;
      while (i < line.size()) {
        if (line[i] == '[') (void)class_matches(line, i, '\0');
        else ++i;
      }
    }
    // A leading slash makes an otherwise basename-shaped rule root-relative;
    // retain anchored here because matching uses basename as the fast path.
    rule.basename = !rule.anchored && line.find('/') == std::string::npos;
    rule.pattern = std::move(line);
    return rule;
  }

  void add_rule(const std::string &line, const std::string &where) {
    Rule rule = parse_rule(line, where);
    if (!rule.pattern.empty()) rules_.push_back(std::move(rule));
  }

  static bool matches(const Rule &rule, const std::string &candidate, bool directory) {
    std::vector<std::string_view> parts;
    size_t begin = 0;
    while (begin <= candidate.size()) {
      const size_t slash = candidate.find('/', begin);
      parts.emplace_back(candidate.data() + begin, (slash == std::string::npos ? candidate.size() : slash) - begin);
      if (slash == std::string::npos) break;
      begin = slash + 1;
    }
    if (rule.basename) {
      const size_t count = rule.directory && !directory && !parts.empty() ? parts.size() - 1 : parts.size();
      for (size_t i = 0; i < count; ++i)
        if (glob(rule.pattern, parts[i])) return true;
      return false;
    }
    std::vector<std::string> candidates;
    // A pattern matching a directory excludes its descendants even without
    // a trailing slash. Slash-containing patterns stay rooted at the scan root.
    size_t end = 0;
    for (size_t n = 0; n < parts.size(); ++n) {
      end = (n ? end + 1 : 0) + parts[n].size();
      candidates.push_back(candidate.substr(0, end));
    }
    if (rule.directory && !directory && !candidates.empty()) candidates.pop_back();
    for (const auto &text : candidates) {
      if (glob(rule.pattern, text)) return true;
    }
    return false;
  }

public:
  explicit IgnoreRules(std::filesystem::path root, std::vector<std::string> extra = {}) {
    std::error_code ec;
    root_ = std::filesystem::absolute(std::move(root), ec).lexically_normal();
    if (ec) throw std::runtime_error("cannot resolve ignore root: " + ec.message());
    const auto file = root_ / ".lmokignore";
    std::ifstream in(file, std::ios::binary);
    if (!in) {
      const auto status = std::filesystem::status(file, ec);
      // status() reports no_such_file through ec on some libstdc++ versions;
      // an absent ignore file is the normal, empty-rules case.
      if (ec == std::make_error_code(std::errc::no_such_file_or_directory)) ec.clear();
      if (ec) throw std::runtime_error("cannot read " + path_text(file) + ": " + ec.message());
      if (std::filesystem::exists(status))
        throw std::runtime_error("cannot read " + path_text(file));
    } else {
      std::string line;
      size_t number = 0;
      while (std::getline(in, line)) add_rule(line, path_text(file) + ":" + std::to_string(++number));
      if (in.bad()) throw std::runtime_error("cannot read " + path_text(file));
    }
    for (size_t i = 0; i < extra.size(); ++i) add_rule(extra[i], "extra[" + std::to_string(i) + "]");
  }

  bool ignored(const std::filesystem::path &candidate, bool directory = false) const {
    std::filesystem::path path = candidate.lexically_normal();
    if (path.is_absolute()) path = path.lexically_relative(root_);
    else path = path.lexically_normal();
    if (path.is_absolute())
      throw std::runtime_error("ignore candidate is outside root: " + path_text(candidate));
    for (const auto &component : path)
      if (component == "..")
        throw std::runtime_error("ignore candidate is outside root: " + path_text(candidate));
    if (path.empty() || path == ".")
      throw std::runtime_error("ignore candidate must name a path below root: " + path_text(candidate));
    const std::string text = path_text(path);
    bool result = false;
    for (const Rule &rule : rules_) if (matches(rule, text, directory)) result = !rule.negated;
    return result;
  }
};

} // namespace lmok
