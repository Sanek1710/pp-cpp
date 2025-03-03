#include "Cursor.h"

#include <unistd.h>

#include <cctype>
#include <functional>
#include <vector>

using iterator = std::string_view::iterator;

namespace {

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

inline bool is_space(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

}  // namespace

void Preprocessor::skip_identifier() {
  for (++cur.it; cur.it != end; ++cur.it) {
    if (!std::isalnum(*cur.it) && *cur.it != '_') return;
  }
}

void Preprocessor::skip_number() {
  for (++cur.it; cur.it != end; ++cur.it) {
    // might be some complicated logic with [eEpP][+-] but meh
    // too uncommon plus next after them has to be number anyway
    if (!std::isalnum(*cur.it) && *cur.it != '_' && *cur.it != '.') return;
  }
}

void Preprocessor::skip_line_comment() {
  for (++ ++cur.it; cur.it != end; ++cur.it) {
    if (*cur.it == '\n') {
      if (*std::prev(cur.it) != '\\') return;
      cur.enter();
    }
  }
}

void Preprocessor::skip_multiline_comment() {
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

void Preprocessor::skip_string_literal(bool ppline) {
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
  skip_identifier();
}

void Preprocessor::skip_include_string() {
  const char quot = *cur.it == '<' ? '>' :  //
                        (*cur.it == '"' ? '"' : 0);
  if (!quot) return;
  for (++cur.it; cur.it != end; ++cur.it) {
    if (*cur.it == quot || *cur.it == '\n') break;
    if (*cur.it == '\n') {
      if (*std::prev(cur.it) != '\\') return;
      cur.enter();
    }
  }
  if (*cur.it != '\n') ++cur.it;
}

template <bool pp_line>
void Preprocessor::process_token() {
  if (is_space(*cur.it)) {
    do {
      if (*cur.it == '\n') {
        cur.enter();
        if (true) {
          ++cur.it;
          cur.clear_line = true;
          return /*newline*/;
        }
      }
    } while (++cur.it, cur.it != end && is_space(*cur.it));
    return /* space */;
  }

  if (*cur.it == '\\') {
    ++cur.it;
    if (cur.it != end && *cur.it == '\n') {
      cur.enter();
      ++cur.it;
      return /* line_continuation */;
    }
    return /*'\\'*/;
  }

  if (*cur.it == '/') {
    if (std::next(cur.it) == end) {
      ++cur.it;
      return;
    }
    if (*std::next(cur.it) == '/') {
      skip_line_comment();
      // process_line_comment
      return;
    } else if (*std::next(cur.it) == '*') {
      skip_multiline_comment();
      // process_multiline_comment
      return;
    }
    cur.clear_line = false;
    ++cur.it;
    return;
  }

  if (std::isalnum(*cur.it) || *cur.it == '_') {
    cur.clear_line = false;
    if (std::isdigit(*cur.it)) {
      skip_number();
      return /* number */;
    }
    const iterator start = cur.it;
    skip_identifier();
    size_t tokan_size = cur.it - start;
    if (cur.it == end) return /* identifier */;
    if (cur.it != end  //
        && (*cur.it == '\'' || *cur.it == '"') &&
        is_string_prefix(std::string_view{start, tokan_size})) {
      skip_string_literal(pp_line);
      return;
    }
    // process_identifier
    return;
  }

  if (*cur.it == '\'' || *cur.it == '"') {
    cur.clear_line = false;
    skip_string_literal(pp_line);
    return;
  }

  if (*cur.it == '#') {
    if (cur.clear_line) {
      // process directive
    }
    ++cur.it;
    return;
  }

  cur.clear_line = false;

  if (*cur.it == '.') {
    ++cur.it;
    if (end - cur.it >= 2  //
        && *cur.it == '.' && *std::next(cur.it) == '.') {
      ++ ++cur.it;
      return /* ellipsis */;
    }
    return /* '.' */;
  }
  ++cur.it;
  return /* *cur.it */;
}

#include "helper.h"

int main(int argc, char* argv[]) {
  timeit;
  checkin;
  //~8.9 Mb
  // std::string src = read_file(ROOT "/pp.test/test.cpp");
  std::string src = read_file(ROOT "/sqliteall.c");
  printit(src.size());
  std::string out;

  if (true) {
    timeit;
    Preprocessor ppm{src};
    ppm.process_code();
    // printit(it.nleft());
  }
  write_file(ROOT "/out.pp.c", out);
  // return true;
  size_t summer = 0;

  if (true) {
    //~8.9 Gb benchmark
    stimeit("process_code 100 times");
    std::string out;
    repeat(10) {
      repeat(10) {
        Preprocessor ppm{src};
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