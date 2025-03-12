#pragma once

#include <cctype>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ankerl/unordered_dense.h"
#include "helper.h"

// char checks
// these work noticibly faster than std's
// table scan masks thigs or however they work

constexpr bool is_space(char c) {  //
  return c == ' ' || '\t' <= c && c <= '\r';
}

constexpr bool is_digit(char c) {
  switch (c) {
    case '0' ... '9':
      return true;
    default:
      break;
  }
  return false;
}

constexpr bool is_word_start_char(char c) {
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

constexpr bool is_word_char(char c) {
  return is_word_start_char(c) || is_digit(c);
}

struct Position {
  uint32_t line;
  uint32_t column;
};

struct Range {
  uint32_t start;
  uint32_t end;
  Position start_pos;
  Position end_pos;
};

struct Cursor {
  using iterator = std::string_view::iterator;

  iterator it;
  iterator line_start_it = 0;
  unsigned nline = 0;
  bool clear_line = true;

  Cursor(iterator begin) : it(begin), line_start_it(it) {}

  void enter() {
    line_start_it = it + 1;
    ++nline;
  }

  Position to_position() const {
    return {.line = nline, .column = static_cast<uint32_t>(it - line_start_it)};
  }
};

using token_id = char;

struct Token {
  Range range;
  token_id id;

  std::string_view get_text(std::string_view src) const {
    return src.substr(range.start, range.end - range.start);
  }

  void print(std::string_view src, std::ostream& os) const {
    os << "[" << std::setw(3) << range.start_pos.line    //
       << ":" << std::setw(2) << range.start_pos.column  //
       << "]: `" << ctrl_str{get_text(src)} << "`\n";
  }

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

class Tokeniser {
  using iterator = std::string_view::iterator;

 public:
  static constexpr uint32_t max_src_size = ~uint32_t{};
  // try:
  struct TokenGroup {
    std::vector<Token> tokens;
  };
  struct DefineTokenGroup : TokenGroup {
    short expansion_offset = 0;
    bool is_variadic = false;
    bool is_functional = false;
  };
  struct IncludeTokenGroup : TokenGroup {};

  struct IncludeImage {
    std::string_view include_str;
  } includeImage;

  struct DefineImage {
    std::string_view name;
    std::vector<std::string_view> args;
    std::vector<std::string_view> expansion;
    bool is_variadic = false;
    bool is_functional = false;

    void clear() {
      args.clear();
      expansion.clear();
      is_variadic = false;
      is_functional = false;
    }
  } defineImage;

  struct UndefImage {
    std::string_view name;
  } undefImage;

  Tokeniser(std::string_view src)  //
      : src{src}, cur{src.begin()}, end{src.end()} {
    if (src.size() > max_src_size) {
      src.remove_suffix(src.size() - max_src_size);
      end = src.end();
    }
  }

  template <bool ppline = false>
  inline Token read_token() {
    Token token;
    token.range.start = cur.it - src.begin();
    token.range.start_pos = cur.to_position();
    token.id = skip_next<ppline>();
    // TODO: maybe peek next and cat with previous
    // if we see line_continuation here
    token.range.end = cur.it - src.begin();
    token.range.end_pos = cur.to_position();
    return token;
  }

 private:
  std::string_view src;
  Cursor cur;
  Cursor::iterator end;

  // skip basic tokens assuming being on their start
  void skip_identifier();
  void skip_number();
  void skip_line_comment();
  void skip_multiline_comment();

  template <bool ppline>
  void skip_string_literal();

  // consume patterns without tokenisation
  bool consume_include_string();
  bool consume_ellipis();
  bool consume_char(char c);
  bool consume_identifier();

  template <bool ppline>
  void skip_ws();

  inline void skip_newline() {
    cur.enter();
    cur.clear_line = true;
    ++cur.it;
  }

  inline void skip(size_t n = 1) {
    cur.clear_line = false;
    cur.it += n;
  }
  inline void skip_line_continuation() {
    ++cur.it;     // '\'
    cur.enter();  // '\n'
    ++cur.it;
  }

  // main token processing unit
  template <bool ppline, bool extras_only = false>
  token_id skip_next();

  template <bool ppline>
  void skip_extras();

  void skip_ppline_extras() { return skip_extras</*ppline=*/true>(); }

  // used to skip preprocessor lines
  void skip_ppline() {
    while (cur.it != end && *cur.it != '\n') skip_next<true>();
  }

  bool process_include();
  bool process_define();
  bool process_undef();

  token_id process_directive();
};

bool is_string_prefix(std::string_view str, bool& is_raw);

template <bool ppline>
void Tokeniser::skip_string_literal() {
  const char quot = *cur.it;
  bool escaped = false;
  for (++cur.it; cur.it != end; ++cur.it) {
    if (ppline && *cur.it == '\n') return;
    if (*cur.it == '\\') {
      if (*std::next(cur.it) == '\n') {
        ++cur.it;
        cur.enter();
        continue;
      }
      escaped = !escaped;
      continue;
    }
    if (escaped) {
      escaped = false;
      continue;
    }
    if (*cur.it == quot) {
      ++cur.it;
      break;
    }
  }
  if (cur.it == end) return;
  consume_identifier();
}

template <bool ppline>
void Tokeniser::skip_ws() {
  do {
    if (*cur.it == '\n') {
      if (ppline) return;
      cur.enter();
      cur.clear_line = true;
    }
    ++cur.it;
  } while (cur.it != end && is_space(*cur.it));
}

template <bool ppline, bool extras_only>
token_id Tokeniser::skip_next() {
  if (cur.it == end) return Token::eof;
  if (is_space(*cur.it)) { /*0*/
    if constexpr (ppline) {
      if (*cur.it == '\n') {
        if constexpr (!extras_only) skip_newline();
        return Token::newline;
      }
    }
    skip_ws<ppline>();
    return Token::space;
  }

  if (is_word_char(*cur.it)) { /*1*/
    if constexpr (extras_only) return Token::identifier;
    cur.clear_line = false;
    if (is_digit(*cur.it)) {
      skip_number();
      return Token::number;
    }
    const iterator start = cur.it;
    skip_identifier();
    const size_t token_size = cur.it - start;
    bool is_raw = false;
    if (cur.it != end                           //
        && (*cur.it == '\'' || *cur.it == '"')  //
        && is_string_prefix(std::string_view{start, token_size}, is_raw)) {
      if (!is_raw) {
        skip_string_literal<ppline>();
        return Token::string_like_literal;
      } else if (*cur.it == '\"') {
        skip_string_literal<ppline>();
        return Token::raw_string_literal;
      }
    }
    return Token::identifier;
  }

  if (*cur.it == '/') { /*2*/
    const auto next_it = std::next(cur.it);
    if (next_it != end) {
      if (*next_it == '/') {
        skip_line_comment();
        return Token::line_comment;
      }
      if (*next_it == '*') {
        skip_multiline_comment();
        return Token::multiline_comment;
      }
    }
    if constexpr (extras_only) return '/';
    cur.clear_line = false;
    skip();
    return '/';
  }

  if (*cur.it == '\'' || *cur.it == '"') { /*3*/
    if constexpr (extras_only) return Token::string_like_literal;
    cur.clear_line = false;
    skip_string_literal<ppline>();
    return Token::string_like_literal;
  }

  if (*cur.it == '\\') { /*4*/
    const auto next_it = std::next(cur.it);
    if (next_it != end && *next_it == '\n') {
      skip_line_continuation();
      return Token::line_continuation;
    }
    if constexpr (extras_only) return '\\';
    skip();
    return '\\';
  }

  if (*cur.it == '#') { /*5*/
    if constexpr (extras_only) return '#';
    ++cur.it;
    if constexpr (ppline) {
      if (cur.it != end && *cur.it == '#') {
        ++cur.it;
        return Token::pp_op_cat;
      }
      return Token::pp_op_str;
    } else {
      if (!cur.clear_line) return '#';
      return process_directive();
    }
  }

  /*7*/
  if constexpr (extras_only) return *cur.it;
  return *cur.it++;
}

// template token_id Tokeniser::skip_next<true, true>();
// template token_id Tokeniser::skip_next<false, false>();
