#pragma once

#include <iomanip>
#include <iostream>
#include <string_view>

#include "ankerl/unordered_dense.h"
#include "helper.h"

struct Position {
  unsigned line;
  unsigned column;
};

struct Cursor {
  using iterator = std::string_view::iterator;

  iterator it;
  iterator line_start_it = 0;
  unsigned nline = 0;
  bool clear_line = false;

  Cursor(iterator begin) : it(begin), line_start_it(it) {}

  void enter() {
    line_start_it = it + 1;
    ++nline;
  }

  Position to_position() {
    return {.line = nline, .column = static_cast<unsigned>(it - line_start_it)};
  }
};

using token_id = char;
struct token {
  // basically character mapping to enum like constants:
  // any space -> ' ', except from '\n'
  // any number -> '0'
  // any identifier -> 'a'
  // line comment -> 'c'
  // multiline comment -> 'm'
  // string -> '"'
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

  static constexpr token_id string = '"';

  static constexpr token_id pp_start = 'p';
  static constexpr token_id pp_op_str = '1';
  static constexpr token_id pp_op_cat = '2';
  static constexpr token_id ellipsis = 'e';

  static constexpr token_id line_continuation = 'z';
};

struct Token {
  using iterator = std::string_view::iterator;
  iterator start;
  iterator end;
  Position pos;
  token_id id;

  std::string_view get_text() const {
    return {start, static_cast<size_t>(end - start)};
  }

  void print(std::ostream& os) const {
    os << "[" << std::setw(3) << pos.line    //
       << ":" << std::setw(2) << pos.column  //
       << "]: `" << ctrl_str{get_text()} << "`\n";
  }
};

template <bool ppline>
inline bool is_extra(token_id token) {
  return token == token::space                 //
         || token == token::multiline_comment  //
         || token == token::line_comment       //
         || token == token::line_continuation  //
         || (!ppline && token == token::newline);
}

class Tokeniser {
 public:
  Tokeniser(std::string_view src)
      : src{src}, cur{src.begin()}, end{src.end()} {}

  ankerl::unordered_dense::set<std::string_view> identifiers;

  void read_token(bool skip_extras = false) {
    read_token_template<false>(skip_extras);
  }
  void read_pptoken(bool skip_extras = false) {
    read_token_template<true>(skip_extras);
  }

  void process_code() {
    while (cur.it < end) {
      read_token();
      // token.print(std::cerr);
      if (token.id == token::eof) break;
      if (token.id == token::identifier) {
        if (auto it = identifiers.find(token.get_text());
            it != identifiers.end()) {
          std::cout << token.get_text();
        }
        // process identifier
        continue;
      }
      if (token.id == token::pp_start) {
        // token.print(std::cerr);
        // read_token(true);
        // token.print(std::cerr);
        // read_token(true);
        // token.print(std::cerr);
        // std::cerr << "#\n";
        // process ppstart
        continue;
      }
    }
  }

 private:
  std::string_view src;
  Cursor cur;
  Cursor::iterator end;

  Token token;

  // skip basic tokens assuming being on their start
  inline void skip_identifier();
  inline void skip_number();
  inline void skip_line_comment();
  inline void skip_multiline_comment();
  inline void skip_string_literal(bool ppline = false);

  // skip special tokens
  inline void skip_include_string();

  template <bool ppline>
  token_id tokenise_next();

  template <bool ppline>
  void read_token_template(bool skip_extras) {
    while (cur.it != end) {
      token.start = cur.it;
      token.pos = cur.to_position();
      token.id = tokenise_next<ppline>();
      token.end = cur.it;
      if (!(skip_extras && is_extra<ppline>(token.id))) return;
    }
    set_eof();
    return;
  }


  bool consume_token(token_id id, bool skip_extras = false) {
    if (token.id != id) return false;
    read_token(skip_extras);
  }
  bool consume_pptoken(token_id id, bool skip_extras = false) {
    if (token.id != id) return false;
    read_token(skip_extras);
  }

  void set_eof() {
    token.start = cur.it;
    token.pos = cur.to_position();
    token.id = token::eof;
    token.end = cur.it;
  }
};