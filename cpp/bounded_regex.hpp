#pragma once
#include <iterator>
#include <regex>
#include <stdexcept>
#include <string>

namespace lmok {
// std::regex has neither a portable execution budget nor a stack bound.
// Charge iterator operations shared by all backtracking copies. This bounds
// both failed searches and recursive matching, without changing regex syntax.
// Budget 65536: libc++ (macOS) charges several times more iterator ops per
// scan position than libstdc++ — a legit anchored-but-failing scan of a
// ~150-char container text trips 4096 there. Catastrophic patterns trip any
// finite budget quickly (exponential backtracking charges fast).
class RegexLimit : public std::runtime_error {
public:
  RegexLimit() : std::runtime_error("regex work limit exceeded (65536)") {}
};
class RegexIterator {
  std::string::const_iterator it_;
  size_t *remaining_;
  void charge() const { if (!*remaining_) throw RegexLimit(); --*remaining_; }
public:
  using iterator_category = std::bidirectional_iterator_tag;
  using value_type = char;
  using difference_type = std::ptrdiff_t;
  using pointer = const char *;
  using reference = const char &;
  RegexIterator() : remaining_(nullptr) {}
  RegexIterator(std::string::const_iterator it, size_t &remaining) : it_(it), remaining_(&remaining) {}
  reference operator*() const { charge(); return *it_; }
  RegexIterator &operator++() { charge(); ++it_; return *this; }
  RegexIterator operator++(int) { auto old = *this; ++*this; return old; }
  RegexIterator &operator--() { charge(); --it_; return *this; }
  RegexIterator operator--(int) { auto old = *this; --*this; return old; }
  bool operator==(const RegexIterator &other) const { if (remaining_) charge(); return it_ == other.it_; }
  bool operator!=(const RegexIterator &other) const { return !(*this == other); }
};
inline bool bounded_regex_search(const std::string &text, const std::regex &regex) {
  size_t remaining = 65536;
  try {
    return std::regex_search(RegexIterator(text.begin(), remaining), RegexIterator(text.end(), remaining), regex);
  } catch (const std::regex_error &) {
    // libc++ has its own match-complexity limit that can fire before our
    // budget and throws std::regex_error at SEARCH time (compile-time syntax
    // errors are already handled at construction); normalize to our error.
    throw RegexLimit();
  }
}
inline std::regex bounded_regex(const std::string &pattern) {
  // Bound recursive compilation and counted-repeat expansion as well. The
  // product is deliberately conservative for independent counted repeats.
  if (pattern.size() > 1024)
    throw std::length_error("regex pattern limit exceeded (1024 bytes)");
  size_t expansion = 1;
  bool escaped = false, in_class = false;
  for (size_t i = 0; i < pattern.size(); ++i) {
    const char c = pattern[i];
    if (escaped) { escaped = false; continue; }
    if (c == '\\') { escaped = true; continue; }
    if (c == '[') in_class = true;
    if (c == ']') in_class = false;
    if (c != '{' || in_class) continue;
    size_t largest = 0, number = 0;
    for (size_t j = i + 1; j < pattern.size() && pattern[j] != '}'; ++j) {
      if (pattern[j] >= '0' && pattern[j] <= '9') {
        number = number * 10 + (pattern[j] - '0');
        if (number > 1024)
          throw std::length_error("regex repetition limit exceeded (1024)");
        if (number > largest) largest = number;
      } else if (pattern[j] == ',') number = 0;
      else break;
    }
    if (largest > 1) expansion *= largest;
    if (expansion > 1024)
      throw std::length_error("regex repetition limit exceeded (1024)");
  }
  return std::regex(pattern);
}
} // namespace lmok
