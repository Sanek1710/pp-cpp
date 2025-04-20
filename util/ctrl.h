#pragma once

#include <ostream>
#include <string>

namespace details {

inline std::ostream& print_char(std::ostream& os, char c) {
  // clang-format off
  switch (c) {
    case '\\': return os << "\\\\";
    case '\0': return os << "\\0";
    case '\a': return os << "\\a";
    case '\b': return os << "\\b";
    case '\t': return os << "\\t";
    case '\n': return os << "\\n";
    case '\v': return os << "\\v";
    case '\f': return os << "\\f";
    case '\r': return os << "\\r";
    default: break;
  }
  // clang-format on
  const unsigned char uc = c;
  if (!std::iscntrl(uc)) return os << c;
  return os << "\\.";
}

}  // namespace details

struct ctrl_str {
  ctrl_str(std::string_view sv) : sv(sv){};
  ctrl_str(char c) : letter(c), sv(&letter, 1){};
  std::string_view sv;
  char letter;
};
inline std::ostream& operator<<(std::ostream& os, const ctrl_str& ctrls) {
  for (char c : ctrls.sv) details::print_char(os, c);
  return os;
}
