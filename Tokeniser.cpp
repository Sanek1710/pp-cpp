#include <cctype>
#include <cstddef>
#include <iostream>
#include <iterator>
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
// codes 0 - 255 reserved for actual chars
token_t last_token = 255;
static constexpr token_t number = 256;
static constexpr token_t identifier = 257;
static constexpr token_t string_literal = 258;
static constexpr token_t line_comment = 259;
static constexpr token_t multiline_comment = 260;
static constexpr token_t space = 261;
static constexpr token_t line_continuation = 262;
};  // namespace token

struct Position {
  unsigned line;
  unsigned column;
};

struct Cursor {
  iterator it;
  iterator line_start_it = 0;
  unsigned nline = 0;

  Cursor(iterator begin) : it(begin) {}

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
    if (*cur.it == '/' && *std::prev(cur.it) == '*') return;
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
token_t read_token(Cursor& cur, iterator end) {
  if (is_space(*cur.it)) {
    do {
      if (*cur.it == '\n') {
        cur.enter();
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
    return *cur.it++;
  }

  if (*cur.it == '#') {
    return *cur.it++;
  }

  return *cur.it++;
}

inline bool is_extra(token_t token) {
  return token == token::space                 //
         || token == token::multiline_comment  //
         || token == token::line_comment       //
         || token == token::line_continuation;
}

std::string scan(Cursor& cur, iterator end) {
  std::string out;
  // out.reserve(end - cur.it);
  while (cur.it != end) {
    iterator start = cur.it;
    token_t token = read_token(cur, end);
    if (token != 0) continue;

    out += std::string_view{start, static_cast<size_t>(cur.it - start)};
    out += "\n";
    // std::cerr << token << " ";
  }
  return out;
}

int main(int argc, char* argv[]) {
  timeit;
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
