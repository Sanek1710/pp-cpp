#include <cstddef>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

#include "MicroPP.h"

std::string read_file(const std::string filename) {
  std::ifstream ifs{filename};
  return std::string{std::istreambuf_iterator<char>{ifs},
                     std::istreambuf_iterator<char>{}};
}

void write_file(const std::string filename, std::string_view s) {
  std::ofstream ofs{filename};
  ofs.write(s.data(), s.size());
}

#define CHECKIN std::cerr << __func__ << "\n"

int main() {
  std::string source_code = read_file("/mnt/d/Projects/pp-cpp/pp.test/test.cpp");
  // std::cerr << source_code << "\n";
  std::string out = MicroParser{source_code}.process();
  // std::string out = parse_code(source_code);
  write_file("/mnt/d/Projects/pp-cpp/pp.test/pp.test.cpp", out);
  // std::cerr << out << "\n";
  return 0;
}