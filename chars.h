#pragma once

// char checks
// these work noticibly faster than std's
// table scan masks thigs or however they work

#include <cctype>
#include <cstdint>
namespace custom {

constexpr inline bool is_space(char c) {  //
  return c == ' ' || '\t' <= c && c <= '\r';
}

constexpr inline bool is_blank(char c) {  //
  return c == ' ' || c == '\t';
}

constexpr inline bool is_digit(char c) {
  switch (c) {
    case '0' ... '9':
      return true;
    default:
      break;
  }
  return false;
}

constexpr inline bool is_word_start_char(char c) {
  switch (c) {
    case 'a' ... 'z':
    case 'A' ... 'Z':
    case '_':
      return true;
    default:
      break;
  }
  return false;
}

constexpr inline bool is_word_char(char c) {
  return is_word_start_char(c) || is_digit(c);
}

// this actually a tiny bit faster, but doubt it actually worty
constexpr inline bool is_num_char(char c) {
  switch (c) {
    case '\'':
    case '.':
    case '0' ... '9':
    case 'a' ... 'z':
    case 'A' ... 'Z':
    case '_':
      return true;
    default:
      break;
  }
  return false;
}

}  // namespace custom

namespace advanced {

using cmask = uint8_t;
using cord = uint8_t;

namespace order {
// clang-format off
  static constexpr cord space      = 0;
  static constexpr cord alpha_     = 1;
  static constexpr cord digit      = 2;
  static constexpr cord comment    = 3;
  
  static constexpr cord quot       = 4;
  static constexpr cord dblquot    = 5;
  static constexpr cord backslash  = 6;
  static constexpr cord dot        = 7;
// clang-format on
}  // namespace order

namespace mask {
// clang-format off
static constexpr cmask space      = 1 << order::space  ;
static constexpr cmask alpha_     = 1 << order::alpha_ ;
static constexpr cmask digit      = 1 << order::digit  ;
static constexpr cmask comment    = 1 << order::comment;

static constexpr cmask quot       = 1 << order::quot     ;
static constexpr cmask dblquot    = 1 << order::dblquot  ;
static constexpr cmask backslash  = 1 << order::backslash;
static constexpr cmask dot        = 1 << order::dot      ;

static constexpr cmask word_in    = alpha_ | digit;
static constexpr cmask num_in     = word_in | quot | dot;

// clang-format on
}  // namespace mask

inline const cord* get_char_oder_table() {
  static cord char_order_table[256] = {};
  for (int c = 0; c < 256; ++c) {
    if (std::isspace(c))
      char_order_table[c] = order::space;
    else if (std::isalpha(c) || c == '_')
      char_order_table[c] = order::alpha_;
    else if (std::isdigit(c))
      char_order_table[c] = order::digit;
    else if (c == '/')
      char_order_table[c] = order::comment;
    else if (c == '\'')
      char_order_table[c] = order::quot;
    else if (c == '"')
      char_order_table[c] = order::dblquot;
    else if (c == '\\')
      char_order_table[c] = order::backslash;
    else if (c == '.')
      char_order_table[c] = order::dot;
    else
      char_order_table[c] = 9;
  }
  return char_order_table;
}
inline const cord* char_order_table = get_char_oder_table();

inline cord ord(cmask c) { return char_order_table[c]; }

inline const cmask* get_char_table() {
  static cmask char_table[256] = {};
  for (int c = 0; c < 256; ++c) {
    char_table[c] = 1U << char_order_table[c];
  }
  return char_table;
}

inline const cmask* char_table = get_char_table();

inline bool is_space(cmask c) noexcept {  //
  return char_table[c] & mask::space;
}

inline bool is_digit(cmask c) noexcept { return char_table[c] & mask::digit; }

inline bool is_word_start_char(cmask c) noexcept {
  return char_table[c] & mask::alpha_;
}

inline bool is_word_char(cmask c) noexcept {
  return char_table[c] & mask::word_in;
}

// this actually a tiny bit faster, but doubt it actually worty
inline bool is_num_char(cmask c) noexcept {
  return char_table[c] & mask::num_in;
}

inline bool is_comment_start(cmask c) noexcept {
  return char_table[c] & mask::comment;
}

inline bool is_interesting(cmask c) noexcept { return char_table[c]; }

inline cmask char_mask(cmask c) noexcept { return char_table[c]; }

}  // namespace advanced

using namespace custom;