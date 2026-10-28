#pragma once
#include "dslvm.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace lmok {
inline std::string read_input_file(const std::string &path) {
  const auto native = std::filesystem::u8path(path);
  std::error_code ec;
  // Directories get the explicit suffix; other non-regular files (devices,
  // FIFOs — opening a FIFO can block) are refused with the plain message.
  if (std::filesystem::is_directory(native, ec))
    throw dslvm::Error("cannot read " + path + ": is a directory");
  if (!std::filesystem::is_regular_file(native, ec))
    throw dslvm::Error("cannot read " + path);
  std::ifstream in(native, std::ios::binary);
  if (!in) throw dslvm::Error("cannot read " + path);
  std::ostringstream data;
  char buffer[8192];
  while (in.read(buffer, sizeof buffer)) data.write(buffer, sizeof buffer);
  data.write(buffer, in.gcount());
  if (!in.eof() || in.bad()) throw dslvm::Error("cannot read " + path);
  return data.str();
}
} // namespace lmok
