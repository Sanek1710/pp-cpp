#pragma once

#include <cstddef>
#include <cstdint>
#include <ostream>

#include "Position.h"

using Tag = uint16_t;

namespace tag {

enum class Kind : uint8_t {
  // one single char as is
  raw = 0,
  // spaces comments etc
  extra = 1,
  // represent 1 and more character
  grouped = 2,
  // preprocessor line specific tokens
  ppline = 4,
  // auxionry tokens for custom kinds
  aux = 8,
  // Token owning allocated string
  macro_arg = 16,
  // punctuators
  punct = 32,
};

inline static constexpr Tag group(char id, Kind category) {
  return (static_cast<Tag>(category) << 8U)  //
         | static_cast<Tag>(static_cast<uint8_t>(id));
}
inline static constexpr char markerof(Tag tag) {
  return static_cast<char>(tag & 0xFF);
}
inline static constexpr Kind kindof(Tag tag) {
  return static_cast<Kind>(tag >> 8U);
}

inline static constexpr Tag raw(char marker) {
  return group(marker, Kind::raw);
}
inline static constexpr Tag aux(char marker) {
  return group(marker, Kind::aux);
}

inline static constexpr bool is_extra(Tag tag) {
  return kindof(tag) == Kind::extra;
}
inline static constexpr bool is_raw(Tag tag) {
  return kindof(tag) == Kind::raw;
}
inline static constexpr bool is_ppline(Tag tag) {
  return kindof(tag) == Kind::ppline;
}
inline static constexpr bool is_macro_arg(Tag tag) {
  return kindof(tag) == Kind::macro_arg;
}
inline static constexpr bool is_punct(Tag tag) {
  return kindof(tag) == Kind::punct;
}

// basically character mapping to enum like constants:
// `any space         -> ' ', except from '\n'`
// `any number        -> '0'`
// `any identifier    -> 'a'`
// `line comment      -> 'c'`
// `multiline comment -> 'm'`
// `string            -> '"'`
//
// all the others maps to themselves
// potentially allows to build string of tokens
// and apply some pattern matching
// e.g.:
// `MACRO(arg, arg)` -> `a(a, a)`
// `int var = 5` -> `a a = 0`
// `const char* str = "string"` -> `a a* a = "`

// clang-format off
static constexpr Tag eof                 = group('\0', Kind::raw);

static constexpr Tag punct1              = group('1', Kind::punct);
static constexpr Tag punct2              = group('2', Kind::punct);
static constexpr Tag punct3              = group('3', Kind::punct);
static constexpr Tag punct4              = group('4', Kind::punct);

// should not be considered as tokens by pp logic
static constexpr Tag space               = group(' ', Kind::extra);
static constexpr Tag newline             = group('\n', Kind::extra);
static constexpr Tag line_comment        = group('c', Kind::extra);
static constexpr Tag multiline_comment   = group('m', Kind::extra);
static constexpr Tag line_continuation   = group('z', Kind::extra);

static constexpr Tag number              = group('0', Kind::grouped);
static constexpr Tag identifier          = group('a', Kind::grouped);
// static constexpr Tag dollar              = group('$', Kind::grouped);

static constexpr Tag string_like_literal = group('"', Kind::grouped);
static constexpr Tag raw_string_literal  = group('R', Kind::grouped);
static constexpr Tag char_literal        = group('\'', Kind::grouped);
static constexpr Tag ellipsis            = group('e', Kind::grouped);

static constexpr Tag pp_op_str           = group('1', Kind::ppline);
static constexpr Tag pp_op_cat           = group('2', Kind::ppline);

static constexpr Tag pp_start            = group('p', Kind::ppline);
static constexpr Tag pp_define           = group('D', Kind::ppline);
static constexpr Tag pp_include          = group('I', Kind::ppline);
static constexpr Tag pp_undef            = group('U', Kind::ppline);
static constexpr Tag pp_other_directive  = group('O', Kind::ppline);
static constexpr Tag pp_error            = group('E', Kind::ppline);

static constexpr Tag pp_include_string   = group('i', Kind::ppline);

// aux tokens
static constexpr Tag code                = group('C', Kind::aux);
static constexpr Tag other               = group('O', Kind::aux);
static constexpr Tag empty               = group('\0', Kind::aux);

static constexpr Tag arg                 = group('a', Kind::macro_arg);
static constexpr Tag arg_raw             = group('r', Kind::macro_arg);
static constexpr Tag arg_str             = group('s', Kind::macro_arg);

// clang-format on

}  // namespace tag

using positer = std::string_view::iterator;

union TokenDetails {
  uint16_t index;
  struct {
    bool is_expansion;
    bool is_not_macro;
  };
};

// end_pos is never used, why to even store it?
// end pos can be calculated by traversing text, if reeealy needed
#define ENDPOS

struct Token {
  Tag tag;
  TokenDetails details;
  uint32_t size = 0;
  positer start = nullptr;
  Position start_pos;
#ifdef ENDPOS
  Position end_pos;
#endif

  inline constexpr std::string_view get_text() const {
    return std::string_view(start, size);
  }
  inline constexpr positer begin() const { return start; }
  inline constexpr positer end() const { return start + size; }

  void print(std::ostream& os) const;
};
