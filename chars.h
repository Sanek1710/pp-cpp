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

namespace mask {
// clang-format off
static constexpr uint8_t space      = 0b00000001;
static constexpr uint8_t alpha_     = 0b00000010;
static constexpr uint8_t digit      = 0b00000100;
static constexpr uint8_t comment    = 0b00001000;

static constexpr uint8_t quot       = 0b00010000;
static constexpr uint8_t dblquot    = 0b00100000;
static constexpr uint8_t backslash  = 0b01000000;
static constexpr uint8_t dot        = 0b10000000;

static constexpr uint8_t word_in    = alpha_ | digit;
static constexpr uint8_t num_in     = word_in | quot | dot;

// clang-format on
}  // namespace mask

inline const uint8_t* get_char_table() {
  static uint8_t char_table_base[256] = {};
  for (int c = 0; c < 256; ++c) {
    if (std::isspace(c))
      char_table_base[c] = mask::space;
    else if (std::isalpha(c) || c == '_')
      char_table_base[c] = mask::alpha_;
    else if (std::isdigit(c))
      char_table_base[c] = mask::digit;
    else if (c == '/')
      char_table_base[c] = mask::comment;
    else if (c == '\'')
      char_table_base[c] = mask::quot;
    else if (c == '"')
      char_table_base[c] = mask::dblquot;
    else if (c == '\\')
      char_table_base[c] = mask::backslash;
    else if (c == '.')
      char_table_base[c] = mask::dot;
    else
      char_table_base[c] = 0;
  }
  return char_table_base;
}
inline const uint8_t* char_table = get_char_table();

inline bool is_space(uint8_t c) noexcept {  //
  return char_table[c] & mask::space;
}

inline bool is_digit(uint8_t c) noexcept { return char_table[c] & mask::digit; }

inline bool is_word_start_char(uint8_t c) noexcept {
  return char_table[c] & mask::alpha_;
}

inline bool is_word_char(uint8_t c) noexcept {
  return char_table[c] & mask::word_in;
}

// this actually a tiny bit faster, but doubt it actually worty
inline bool is_num_char(uint8_t c) noexcept {
  return char_table[c] & mask::num_in;
}

inline bool is_comment_start(uint8_t c) noexcept {
  return char_table[c] & mask::comment;
}

inline bool is_interesting(uint8_t c) noexcept { return char_table[c]; }

}  // namespace advanced

using namespace advanced;