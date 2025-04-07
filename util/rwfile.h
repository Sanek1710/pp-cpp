#pragma once

#include <fstream>
#include <string>

inline std::string read_file(const std::string filename) {
  std::ifstream ifs{filename};
  return std::string{std::istreambuf_iterator<char>{ifs},
                     std::istreambuf_iterator<char>{}};
}

inline void write_file(const std::string filename, std::string_view s) {
  std::ofstream ofs{filename};
  ofs.write(s.data(), s.size());
}
