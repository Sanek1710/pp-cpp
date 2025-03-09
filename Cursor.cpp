#include "Cursor.h"

#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <functional>
#include <ostream>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "helper.h"

using iterator = std::string_view::iterator;

namespace {

constexpr static bool is_string_prefix(std::string_view str, bool& is_raw) {
  str.remove_suffix(is_raw = str.back() == 'R');
  switch (str.size()) {
    case 0:
      return true;
    case 1:
      return str[0] == 'L' || str[0] == 'U' || str[0] == 'u';
    case 2:
      return str[0] == 'u' && str[1] == '8';
    default:
      break;
  }
  return false;
}

inline bool is_space(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

}  // namespace

void Tokeniser::skip_identifier() {
  for (++cur.it; cur.it != end; ++cur.it) {
    if (!std::isalnum(*cur.it) && *cur.it != '_') return;
  }
}

void Tokeniser::skip_number() {
  for (++cur.it; cur.it != end; ++cur.it) {
    // might be some complicated logic with [eEpP][+-] but meh
    // too uncommon plus next after them has to be number anyway
    if (!std::isalnum(*cur.it) && *cur.it != '_' && *cur.it != '.') return;
  }
}

void Tokeniser::skip_line_comment() {
  for (++ ++cur.it; cur.it != end; ++cur.it) {
    if (*cur.it == '\n') {
      if (*std::prev(cur.it) != '\\') return;
      cur.enter();
    }
  }
}

void Tokeniser::skip_multiline_comment() {
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

bool Tokeniser::consume_include_string() {
  const char quot = *cur.it == '<' ? '>' :  //
                        (*cur.it == '"' ? '"' : 0);
  if (!quot) return false;
  for (++cur.it; cur.it != end; ++cur.it) {
    if (*cur.it == quot || *cur.it == '\n') break;
    if (*cur.it == '\n') {
      if (*std::prev(cur.it) != '\\') return true;  // or false idk
      cur.enter();
    }
  }
  if (*cur.it != '\n') ++cur.it;
  return true;
}

bool Tokeniser::consume_ellipis() {
  if (*cur.it != '.'       //
      || end - cur.it < 3  //
      || cur.it[1] != '.' || cur.it[2] != '.')
    return false;
  ++ ++cur.it;
  return true;
}

bool Tokeniser::consume_char(char c) {
  if (*cur.it != c) return false;
  ++cur.it;
  return true;
}

bool Tokeniser::consume_identifier() {
  if (!(std::isalpha(*cur.it) || *cur.it == '_')) return false;
  skip_identifier();
  return true;
}

void Tokeniser::skip_newline() {
  cur.enter();
  cur.clear_line = true;
  ++cur.it;
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

template <bool ppline>
void Tokeniser::skip_extras() {
  while (is_extra<ppline>(  //
      skip_next<ppline, /*extras_only*/ true>()))
    ;
}

// #define debug

int main(int argc, char* argv[]) {
  timeit;
  checkin;
//~8.9 Mb
#ifdef debug
  std::string src = read_file(ROOT "/Cursor.h");
  src += read_file(ROOT "/helper.h");
#else
  std::string src = read_file(ROOT "/sqliteall.c");
#endif
  printit(src.size());
  std::string out;

  if (true) {
    timeit;
    Tokeniser ppm{src};
    ppm.process_code();
    // printit(it.nleft());
  }
  write_file(ROOT "/out.pp.c", out);
#ifdef debug
  return true;
#endif
  size_t summer = 0;

  if (true) {
    //~890 Mb benchmark
    stimeit("process_code 100 times");
    std::string out;
    repeat(10) {
      repeat(10) {
        Tokeniser ppm{src};
        ppm.process_code();
      }
      // untimeit;
      // usleep(200000);
    }
    summer += out.size();
  }

  printit(out.size());
  printit(summer);
  return 0;
}