#pragma once
#include "host.hpp"
#include <string>
#include <vector>

namespace lmok {
struct FileMetrics { int64_t lines=0, functions=0, max_nesting=0; };
FileMetrics measure_file(const Tree &tree, const std::string &source);
struct Duplicate { int64_t start_row=0,start_col=0,end_row=0,end_col=0,tokens=0; };
std::vector<Duplicate> duplicate_windows(const Tree &tree, int64_t min_tokens);
std::string metrics_json(const std::string &path, const FileMetrics &m);
std::string duplicate_json(const std::string &path, const Duplicate &d);
}
