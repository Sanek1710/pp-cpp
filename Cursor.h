#pragma once

#include <cctype>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

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
struct token {
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
constexpr uint32_t mask3(Cursor::iterator it, Cursor::iterator end) {
  uint32_t val = 0;
  for (int i = 0; i < 4 && it != end; ++i, ++it) {
    val = (val << 8) | *it;
    if (*it == '"' || *it == '\'') return val;
  }
  return 0;
}
constexpr uint32_t mask3(std::string_view sv) {
  return mask3(sv.begin(), sv.end());
}

constexpr auto m1 = mask3("u\"1231231213213");
constexpr auto m2 = mask3("u\"12312sdvsdv13");

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
    std::string_view text = get_text();
    auto clr = [text, this]() -> const char* {
      std::unordered_set<std::string_view> keywords = {
          "return",  "if",     "using", "while", "do",    "break",
          "else",    "for",    "#",     "ifdef", "endif", "else",
          "include", "define", "{",     "}",     "case",  "switch"};
      std::unordered_set<std::string_view> keywords2 = {
          "auto",        "bool",   "char",   "class",    "const",
          "constexpr",   "inline", "int",    "public",   "short",
          "static_cast", "static", "struct", "template", "this",
          "unsigned",    "void",   "false",  "true",     "namespace",
          "assert"};
      std::unordered_set<std::string_view> keywords3 = {
          "std",     "string",   "size_t", "string_view", "unordered_set",
          "ostream", "iterator", "auto",   "uint32_t",    "vector"};
      if (text.empty()) return "";
      if (id == token::line_comment || id == token::multiline_comment)
        return "\033[38;5;22m";
      if (id == token::string_like_literal) return "\033[38;5;216m";
      if (keywords.count(text)) return "\033[38;5;176m";
      if (keywords2.count(text)) return "\033[38;5;75m";
      if (keywords3.count(text)) return "\033[38;5;37m";
      if (std::isdigit(text.front())) return "\033[38;5;193m";
      if (text.front() == '(' || text.front() == ')') return "\033[38;5;228m";
      if (std::ispunct(text.front())) return "\033[38;5;248m";
      if (id == token::identifier && *end == ':') return "\033[38;5;37m";
      if (id == token::identifier && *end == '(') return "\033[38;5;230m";

      return "\033[38;5;153m";
    };
    static unsigned old_line = -1;
    if (old_line != pos.line) {
      os << "\033[38;5;240m" << std::setw(3) << pos.line + 1 << "│ ";
      old_line = pos.line;
    }
    os << clr() << text << "\033[0m";
    // os << "[" << std::setw(3) << pos.line    //
    //    << ":" << std::setw(2) << pos.column  //
    //    << "]: `" << clr() << ctrl_str{text} << "\033[0m`\n";
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
  using iterator = std::string_view::iterator;

 public:
  Tokeniser(std::string_view src)
      : src{src}, cur{src.begin()}, end{src.end()} {}

  ankerl::unordered_dense::set<std::string_view> identifiers;

  // void read_token(bool skip_extras = false) {
  //   read_token_template<false>(skip_extras);
  // }
  // void read_pptoken(bool skip_extras = false) {
  //   read_token_template<true>(skip_extras);
  // }

  void process_code() {
    // static bool halt = false;
    while (true) {
      // if (halt) break;
      // skip_extras<false>();
      read_token();
      // token.print(std::cerr);
      // token.print(std::cerr);
      // token.print(std::cerr);
      // valacer(token.id, 100) {
      //   token.print(std::cerr);
      //   halt = true;
      //   break;
      // }
      // token.print(std::cerr);

      // if (token.pos.line > 258200) {
      //   token.print(std::cerr);
      // }
      if (token.id == token::eof) break;
      if (token.id == token::identifier) {
        // if (auto it = identifiers.find(token.get_text());
        //     it != identifiers.end()) {
        //   std::cout << token.get_text();
        // }
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
  void skip_identifier();
  void skip_number();
  void skip_line_comment();
  void skip_multiline_comment();

  template <bool ppline>
  void skip_string_literal();

  // consume patterns without tokenisation

  // special
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

  void skip_ppline() {
    while (token.id != token::newline  //
           && token.id != token::eof)
      read_token</*ppline=*/true>();
  }

  template <bool ppline = false>
  inline void read_token() {
    token.start = cur.it;
    token.pos = cur.to_position();
    token.id = skip_next<ppline>();
    token.end = cur.it;
  }

  bool process_include();
  bool process_define();
  token_id process_directive();
};

bool is_string_prefix(std::string_view str, bool& is_raw);
inline bool is_space(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

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
  if (cur.it == end) return token::eof;
  if (is_space(*cur.it)) { /*0*/
    if constexpr (ppline) {
      if (*cur.it == '\n') {
        if constexpr (!extras_only) skip_newline();
        return token::newline;
      }
    }
    skip_ws<ppline>();
    return token::space;
  }

  if (std::isalnum(*cur.it) || *cur.it == '_') { /*1*/
    if constexpr (extras_only) return token::identifier;
    cur.clear_line = false;
    if (std::isdigit(*cur.it)) {
      skip_number();
      return token::number;
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
        return token::string_like_literal;
      } else if (*cur.it == '\"') {
        skip_string_literal<ppline>();
        return token::raw_string_literal;
      }
    }
    return token::identifier;
  }

  if (*cur.it == '/') { /*2*/
    const auto next_it = std::next(cur.it);
    if (next_it != end) {
      if (*next_it == '/') {
        skip_line_comment();
        return token::line_comment;
      }
      if (*next_it == '*') {
        skip_multiline_comment();
        return token::multiline_comment;
      }
    }
    if constexpr (extras_only) return '/';
    cur.clear_line = false;
    skip();
    return '/';
  }

  if (*cur.it == '\'' || *cur.it == '"') { /*3*/
    if constexpr (extras_only) return token::string_like_literal;
    cur.clear_line = false;
    skip_string_literal<ppline>();
    return token::string_like_literal;
  }

  if (*cur.it == '\\') { /*4*/
    const auto next_it = std::next(cur.it);
    if (next_it != end && *next_it == '\n') {
      skip_line_continuation();
      return token::line_continuation;
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
        return token::pp_op_cat;
      }
      return token::pp_op_str;
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
