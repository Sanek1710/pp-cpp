#include <cstddef>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

#include "MicroPP.h"

#include "helper.h"

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