#pragma once

#include <cstddef>
#include <cstdint>
#include <ostream>

#include "Position.h"
#include "helper.h"

namespace tkgroup {
// one single char as is
static constexpr char raw = 0;
// spaces comments etc
static constexpr char extra = 1;
// represent 1 and more character
static constexpr char grouped = 2;
// preprocessor line specific tokens
static constexpr char ppline = 4;
}  // namespace tkgroup

struct token_id {
  char id;
  char category;

  inline constexpr bool operator==(const token_id& other) const {
    return id == other.id && category == other.category;
  }
  inline constexpr bool operator!=(const token_id& other) const {
    return !(*this == other);
  }
  inline constexpr bool operator==(const char& other_id) const {
    return id == other_id && category == 0;
  }
  inline constexpr bool operator!=(const char& other_id) const {
    return !(*this == other_id);
  }

  constexpr operator int() const {
    return (static_cast<int>(category) << 8U) | id;
  }
};

struct Token {
  token_id id;
  // uint16_t _alignment;
  Range range;

  inline std::string_view get_text(std::string_view src) const {
    return src.substr(range.start, range.end - range.start);
  }

  void print(std::string_view src, std::ostream& os) const;
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

  static constexpr token_id eof{'\0', tkgroup::raw};

  static constexpr token_id space{' ', tkgroup::extra};
  static constexpr token_id newline{'\n', tkgroup::extra};

  static constexpr token_id number{'0', tkgroup::grouped};
  static constexpr token_id identifier{'a', tkgroup::grouped};

  static constexpr token_id line_comment{'c', tkgroup::extra};
  static constexpr token_id multiline_comment{'m', tkgroup::extra};

  static constexpr token_id string_like_literal{'"', tkgroup::grouped};
  static constexpr token_id raw_string_literal{'R', tkgroup::grouped};
  // static constexpr token_id char_literal{'\'', 0};

  static constexpr token_id ellipsis{'e', tkgroup::grouped};

  static constexpr token_id line_continuation{'z', tkgroup::extra};

  static constexpr token_id pp_start{'p', tkgroup::ppline};
  static constexpr token_id pp_op_str{'1', tkgroup::ppline};
  static constexpr token_id pp_op_cat{'2', tkgroup::ppline};

  static constexpr token_id pp_define{'D', tkgroup::ppline};
  static constexpr token_id pp_include{'I', tkgroup::ppline};
  static constexpr token_id pp_include_string{'i', tkgroup::ppline};
  static constexpr token_id pp_undef{'U', tkgroup::ppline};
  static constexpr token_id pp_other_directive{'O', tkgroup::ppline};
  static constexpr token_id pp_error{'E', tkgroup::ppline};

  static constexpr token_id code_chunk{'C', tkgroup::grouped};
};

inline bool is_extra(token_id token) {
  return token.category == tkgroup::extra;
}
