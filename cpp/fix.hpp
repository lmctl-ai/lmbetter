#pragma once

#include "host.hpp"

#include <cstddef>
#include <map>
#include <string>

namespace lmok {

struct FixEdit {
  int64_t start_byte = 0;
  int64_t end_byte = 0;
  std::string text;
  Grammar grammar = Grammar::Json;
};

struct FixResult {
  std::string text;
  size_t fixed = 0;
  // Parsed trees are retained for the one post-fix lint pass. Tree is
  // movable and owns the source backing the node handles.
  std::map<Grammar, Tree> verified_trees;
};

} // namespace lmok
