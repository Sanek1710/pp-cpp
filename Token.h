#pragma once

#include <cstddef>
#include <cstdint>
#include <ostream>

#include "Position.h"
#include "helper.h"

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
};

inline static constexpr Tag group(char id, Kind category) {
  return (static_cast<Tag>(category) << 8U)  //
         | static_cast<Tag>(static_cast<uint8_t>(id));
}
inline static constexpr Tag raw(char id) { return group(id, Kind::raw); }
inline static constexpr Tag aux(char id) { return group(id, Kind::aux); }

inline static constexpr Kind kindof(Tag tag) {
  return static_cast<Kind>(tag >> 8U);
}

inline static constexpr bool is_extra(Tag tag) {
  return kindof(tag) == Kind::extra;
}
inline static constexpr bool is_raw(Tag tag) {
  return kindof(tag) == Kind::raw;
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

static constexpr Tag space               = group(' ', Kind::extra);
static constexpr Tag newline             = group('\n', Kind::extra);
static constexpr Tag line_comment        = group('c', Kind::extra);
static constexpr Tag multiline_comment   = group('m', Kind::extra);
static constexpr Tag line_continuation   = group('z', Kind::extra);

static constexpr Tag number              = group('0', Kind::grouped);
static constexpr Tag identifier          = group('a', Kind::grouped);
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
static constexpr Tag arg                 = group('A', Kind::aux);
static constexpr Tag other               = group('O', Kind::aux);

// clang-format on

}  // namespace tag

using positer = std::string_view::iterator;

struct Token {
  Tag tag;
  uint16_t external_index;
  uint32_t size = 0;
  positer start = nullptr;
  Position start_pos;
  Position end_pos;

  inline constexpr std::string_view get_text() const {
    return std::string_view(start, size);
  }
  inline constexpr positer begin() const { return start; }
  inline constexpr positer end() const { return start + size; }

  void print(std::ostream& os) const;
};

constexpr Token code_token(std::string_view text, Position start_pos) {
  return Token{.tag = tag::code,
               .external_index = 0,
               .size = static_cast<uint32_t>(text.size()),
               .start = text.data(),
               .start_pos = start_pos,
               .end_pos = start_pos};
}
