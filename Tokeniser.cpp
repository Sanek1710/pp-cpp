#include <cctype>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <ostream>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>

#include "helper.h"

using iterator = std::string_view::iterator;
using token_t = int;

#define tval(name, val) static constexpr token_t name = val

// basically enum but just to avoid casing all over the place
namespace token {
static constexpr token_t eof = 0;
static constexpr token_t space = ' ';
static constexpr token_t newline = '\n';

// codes 0 - 255 reserved for actual chars
static constexpr token_t number = 256;
static constexpr token_t identifier = 257;
static constexpr token_t string_literal = 258;
static constexpr token_t line_comment = 259;
static constexpr token_t multiline_comment = 260;
static constexpr token_t line_continuation = 261;
static constexpr token_t directive_start = 262;
};  // namespace token

struct Position {
  unsigned line;
  unsigned column;
};

struct Cursor {
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

inline void skip_identifier(Cursor& cur, iterator end) {
  for (++cur.it; cur.it != end; ++cur.it) {
    if (!std::isalnum(*cur.it) && *cur.it != '_') return;
  }
}
inline void skip_number_like(Cursor& cur, iterator end) {
  for (++cur.it; cur.it != end; ++cur.it) {
    // might be some complicated logic with [eEpP][+-] but meh
    // too uncommon plus next after them has to be number anyway
    if (!std::isalnum(*cur.it) && *cur.it != '_' && *cur.it != '.') return;
  }
}

inline void skip_line_comment(Cursor& cur, iterator end) {
  for (++ ++cur.it; cur.it != end; ++cur.it) {
    if (*cur.it == '\n') {
      if (*std::prev(cur.it) != '\\') return;
      cur.enter();
    }
  }
}
inline void skip_multiline_comment(Cursor& cur, iterator end) {
  if (++ ++cur.it == end) return;
  if (*cur.it == '\n') cur.enter();
  for (++cur.it; cur.it != end; ++cur.it) {
    if (*cur.it == '/' && *std::prev(cur.it) == '*') {
      ++cur.it;
      return;
    }
    if (*cur.it == '\n') cur.enter();
  }
}

inline void skip_string_like_literal(Cursor& cur, iterator end,
                                     bool ppline = false) {
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
  if (!std::isalpha(*cur.it) && *cur.it != '_') return;
  skip_identifier(cur, end);
}

inline static bool is_string_prefix(std::string_view str) {
  if (str.size() > 3) return false;
  bool isRawString = str.back() == 'R';
  if (isRawString) str.remove_suffix(1);
  switch (str.size()) {
    case 0:
      return true;
    case 1:
      return str.front() == 'L' || str.front() == 'u' || str.front() == 'U';
    case 2:
      return str == "u8";
    default:
      return false;
  }
}
bool my_isspace(char ch) {
  return std::isspace(static_cast<unsigned char>(ch));
}
// custom as we dont need all those \v \f things
// plus ' ' itself hits significantly more often
inline bool is_space(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

template <bool in_pp_directive = false>
token_t skip_token(Cursor& cur, iterator end) {
  if (is_space(*cur.it)) {
    do {
      if (*cur.it == '\n') {
        cur.enter();
        if (true) {
          ++cur.it;
          cur.clear_line = true;
          return token::newline;
        }
      }
    } while (++cur.it, cur.it != end && is_space(*cur.it));
    return token::space;
  }

  if (*cur.it == '\\') {
    ++cur.it;
    if (cur.it != end && *cur.it == '\n') {
      cur.enter();
      ++cur.it;
      return token::line_continuation;
    }
    return '\\';
  }

  if (std::isalnum(*cur.it) || *cur.it == '_') {
    cur.clear_line = false;
    if (std::isdigit(*cur.it)) {
      skip_number_like(cur, end);
      return token::number;
    }
    const iterator start = cur.it;
    skip_identifier(cur, end);
    size_t tokan_size = cur.it - start;
    if (cur.it == end) return token::identifier;
    if ((*cur.it == '\'' || *cur.it == '"') &&
        is_string_prefix(std::string_view{start, tokan_size})) {
      skip_string_like_literal(cur, end);
      return token::string_literal;
    }
    return token::identifier;
  }

  if (*cur.it == '\'' || *cur.it == '"') {
    cur.clear_line = false;
    skip_string_like_literal(cur, end);
    return token::string_literal;
  }

  if (*cur.it == '/') {
    if (std::next(cur.it) == end) return *cur.it++;
    if (*std::next(cur.it) == '/') {
      skip_line_comment(cur, end);
      return token::line_comment;
    } else if (*std::next(cur.it) == '*') {
      skip_multiline_comment(cur, end);
      return token::multiline_comment;
    }
    cur.clear_line = false;
    return *cur.it++;
  }

  if (*cur.it == '#') {
    return *cur.it++;
  }

  cur.clear_line = false;
  return *cur.it++;
}

struct Token {
  iterator start;
  iterator end;
  Position pos;
  token_t kind;

  std::string_view get_text() const {
    return {start, static_cast<size_t>(end - start)};
  }

  void print(std::ostream& os) const {
    os << "[" << std::setw(3) << pos.line    //
       << ":" << std::setw(2) << pos.column  //
       << "]: `" << ctrl_str{get_text()} << "`\n";
  }
};

inline bool is_extra(token_t token) {
  return token == token::space                 //
         || token == token::multiline_comment  //
         || token == token::line_comment       //
         || token == token::line_continuation  //
         || (token == token::newline);
}

Token read_token_skip_extras(Cursor& cur, iterator end) {
  Token token;
  while (cur.it != end) {
    token.start = cur.it;
    token.pos = cur.to_position();
    token.kind = skip_token(cur, end);
    token.end = cur.it;
    if (is_extra(token.kind)) continue;
    return token;
  }
  return Token{.start = cur.it, .pos = cur.to_position(), .kind = token::eof};
}

Token read_token(Cursor& cur, iterator end) {
  if (cur.it == end)
    return Token{.start = cur.it, .pos = cur.to_position(), .kind = token::eof};
  Token token;
  token.start = cur.it;
  token.pos = cur.to_position();
  token.kind = skip_token(cur, end);
  token.end = cur.it;
  return token;
}

std::string scan(Cursor& cur, iterator end) {
  std::string out;
  // out.reserve(end - cur.it);
  size_t total;
  while (true) {
    Token token = read_token_skip_extras(cur, end);
    if (token.kind == token::eof) break;
    total += token.kind;
    // if (token != token::multiline_comment) continue;

    // out += token.get_text();
    // out += "\n";
    // if (cur.clear_line && token.kind == '#') {
    //   token.print(std::cout);
    // }
  }
  out += 'a' + total % 26;
  return out;
}
/**/
int main(int argc, char* argv[]) {
  timeit;
  // std::string src = read_file(ROOT "/Tokeniser.cpp");
  std::string src = read_file(ROOT "/sqliteall.c");
  std::string_view src_view{src};

  size_t total_size = 0;
  {
    timeit;
    repeat(1) {
      Cursor cur{src_view.begin()};
      iterator end = src_view.end();
      std::cerr << scan(cur, end);
      std::cerr << cur.nline << "\n";
    }
  }
  // std::cerr << ctrl_str{'\n'} << "\n";
  // return 0;
  {
    timeit;
    repeat(100) {
      Cursor cur{src_view.begin()};
      iterator end = src_view.end();
      scan(cur, end);
    }
  }
  printit(total_size);

  return 0;
}
