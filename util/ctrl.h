#pragma once

#include <string>

namespace details {

[[deprecated("for the sake of just slashes and cool font")]]  //
inline int
ctrl_encode(unsigned char c) {
  if (c == 0 ||  //
      '\t' <= c && c <= '\r')
    return 224 | c;
  return c;
}

inline auto& appendCtrlEncode(int code, std::string& out) {
  if (!std::iscntrl(code)) return out += code;
  // clang-format off
  switch (code) {
    case '\0': return out += "\\0";
    case '\a': return out += "\\a";
    case '\b': return out += "\\b";
    case '\t': return out += "\\t";
    case '\n': return out += "\\n";
    case '\v': return out += "\\v";
    case '\f': return out += "\\f";
    case '\r': return out += "\\r";
    default: return out += "\\.";
  }
  // clang-format on
}

inline void appendUTF8(int code, std::string& out) {
  if (code < 128) {
    out += static_cast<char>(code);
  } else if (code < 2048) {
    out += static_cast<char>((code >> 6) | 192);
    out += static_cast<char>((code & 63) | 128);
  } else if (code < 65536) {
    out += static_cast<char>((code >> 12) | 224);
    out += static_cast<char>(((code >> 6) & 63) | 128);
    out += static_cast<char>((code & 63) | 128);
  }
}
}  // namespace details

struct ctrl_str {
  ctrl_str(std::string_view sv) : sv(sv){};
  ctrl_str(char c) : letter(c), sv(&letter, 1){};
  std::string_view sv;
  char letter;
};
inline std::ostream& operator<<(std::ostream& os, const ctrl_str& ctrls) {
  std::string out;
  for (unsigned char c : ctrls.sv) {
    // details::appendUTF8(details::ctrl_encode(c), out);
    details::appendCtrlEncode(c, out);
  }
  return os << out;
}
