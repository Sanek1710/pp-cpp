#pragma once

#include <string_view>

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

class Preprocessor {
 public:
  Preprocessor(std::string_view src)
      : src{src}, cur{src.begin()}, end{src.end()} {}

  void process_code() {
    while (cur.it < end) {
      process_token<false>();
    }
  }

 private:
  std::string_view src;
  Cursor cur;
  Cursor::iterator end;

  // skip basic tokens assuming being on their start
  inline void skip_identifier();
  inline void skip_number();
  inline void skip_line_comment();
  inline void skip_multiline_comment();
  inline void skip_string_literal(bool ppline = false);

  // skip special tokens
  inline void skip_include_string();

  template <bool pp_line>
  void process_token();
};