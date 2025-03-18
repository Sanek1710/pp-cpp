#pragma once

#include <ostream>

#include "Position.h"

using token_id = char;

struct Token {
  Range range;
  token_id id;

  inline std::string_view get_text(std::string_view src) const {
    return src.substr(range.start, range.end - range.start);
  }

  void print(std::string_view src, std::ostream& os) const;

  // basically character mapping to enum like constants:
  // `any space -> ' ', except from '\n'`
  // `any number -> '0'`
  // `any identifier -> 'a'`
  // `line comment -> 'c'`
  // `multiline comment -> 'm'`
  // `string -> '"'`
  //
  // all the others maps to themselves
  // potentially allows to build string of tokens
  // and apply some pattern matching
  // e.g.:
  // `MACRO(arg, arg)` -> `a(a, a)`
  // `int var = 5` -> `a a = 0`
  // `const char* str = "string"` -> `a a* a = "`

  static constexpr token_id eof = '\0';

  static constexpr token_id space = ' ';
  static constexpr token_id newline = '\n';

  static constexpr token_id number = '0';
  static constexpr token_id identifier = 'a';

  static constexpr token_id line_comment = 'c';
  static constexpr token_id multiline_comment = 'm';

  static constexpr token_id string_like_literal = '"';
  static constexpr token_id raw_string_literal = 'R';
  // static constexpr token_id char_literal = '\'';

  static constexpr token_id pp_start = 'p';
  static constexpr token_id pp_op_str = '1';
  static constexpr token_id pp_op_cat = '2';
  static constexpr token_id ellipsis = 'e';

  static constexpr token_id line_continuation = 'z';

  static constexpr token_id pp_define = 'D';
  static constexpr token_id pp_include = 'I';
  static constexpr token_id pp_include_string = 'i';
  static constexpr token_id pp_undef = 'U';
  static constexpr token_id pp_other_directive = 'O';
  static constexpr token_id pp_error = 'E';
};

template <bool ppline = false>
inline bool is_extra(token_id token) {
  return token == Token::space                 //
         || token == Token::multiline_comment  //
         || token == Token::line_comment       //
         || token == Token::line_continuation  //
         || (!ppline && token == Token::newline);
}
