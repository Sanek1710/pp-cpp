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

size_t skip_line_comment(std::string_view code, size_t index) {
  std::cerr << __func__ << ": " << index << "\n";
  char c0 = '\0';
  for (size_t i = index + 1; i < code.size(); ++i) {
    char c1 = code[i];
    if (c0 != '\\' && c1 == '\n') {
      return i;
    }
  }
  return index;
}
size_t skip_multiline_comment(std::string_view code, size_t index) {
  std::cerr << __func__ << ": " << index << "\n";
  char c0 = '\0';
  for (size_t i = index + 1; i < code.size(); ++i) {
    char c1 = code[i];
    if (c0 != '*' && c1 == '/') {
      return i;
    }
  }
  return index;
}
size_t skip_string_like_literal(std::string_view code, size_t index) {
  std::cerr << __func__ << ": " << index << "\n";
  char quot = code[index];
  char c0 = '\0';
  for (size_t i = index + 1; i < code.size(); ++i) {
    char c1 = code[i];
    if (c0 != '\\' && c1 == quot) {
      return i;
    }
  }
  return index;
}

void fill_in(std::string& dest, std::string_view src, bool spaces = true) {
  if (spaces)
    dest.append(src.size(), ' ');
  else
    dest.append(src);
}

#include <string.h>

std::string parse_code(std::string_view code) {
  std::string out;
  char c0 = '\0';
  unsigned nline = 0;
  unsigned ncol = 0;

  for (size_t i = 0; i < code.size(); ++i) {
    char c1 = code[i];

    if (c0 == '/' && c1 == '/') {
      int j = skip_line_comment(code, i);
      fill_in(out, code.substr(i, j - i + 1));
      i = j;
      c1 = '\0';
    } else if (c0 == '/' && c1 == '*') {
      int j = skip_multiline_comment(code, i);
      fill_in(out, code.substr(i, j - i + 1));
      i = j;
      c1 = '\0';
    } else if (c1 == '"' || c1 == '\'') {
      int j = skip_string_like_literal(code, i);
      out += code.substr(i, j - i + 1);
      i = j;
      c1 = '\0';
    } else if (c1 == '\n') {
      ++nline;
      ncol = 0;
    }
    if (c1) out += c1;
    c0 = c1;
  }
  return out;
}

int main() {
  std::string source_code = read_file("/mnt/d/Projects/pp-cpp/test.cpp");
  // std::cerr << source_code << "\n";
  std::string out = MicroParser{source_code}.process();
  // std::string out = parse_code(source_code);
  write_file("/mnt/d/Projects/pp-cpp/pp.test.cpp", out);
  // std::cerr << out << "\n";
  return 0;
}