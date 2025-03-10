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

// namespace {

bool is_string_prefix(std::string_view str, bool& is_raw) {
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

// }  // namespace

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
  skip(3);
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

template <bool ppline>
void Tokeniser::skip_extras() {
  while (is_extra<ppline>(  //
      skip_next<ppline, /*extras_only*/ true>()))
    ;
}

bool Tokeniser::process_include() {
  skip_ppline_extras();
  auto start = cur.it;
  if (!consume_include_string()) return false;
  std::string_view include_string(start, cur.it - start);
  skip_ppline();

  // std::cerr << "#include " << include_string << "\n";
  return true;
}

bool Tokeniser::process_define() {
  skip_ppline_extras();
  auto start = cur.it;
  if (!consume_identifier()) return false;
  std::string_view name(start, cur.it - start);
  std::vector<std::string_view> args;
  bool is_variadic = false;

  // not actual loop, just for break outs
  while (consume_char('(')) {
    skip_ppline_extras();
    if (consume_char(')')) break;
    while (true) {
      skip_ppline_extras();
      auto argstart = cur.it;
      bool was_arg = consume_identifier();
      args.emplace_back(argstart, cur.it - argstart);

      skip_ppline_extras();
      if (consume_ellipis()) {
        is_variadic = true;
        skip_ppline_extras();
        if (!consume_char(')')) return false;
        break;
      }

      if (!was_arg) return false;
      if (consume_char(')')) break;
      if (!consume_char(',')) return false;
    }
    break;
  }
  skip_ppline_extras();
  auto expansion_start = cur.it;
  skip_ppline();
  std::string_view expansion(expansion_start, cur.it - expansion_start);

  // std::cerr << "#define " << name;
  // std::cerr << "(";
  // if (!args.empty()) {
  //   auto argit = args.begin();
  //   std::cerr << *argit;
  //   for (++argit; argit != args.end(); ++ argit) {
  //     std::cerr << ", " << *argit;
  //   }
  // }
  // std::cerr << ") " << expansion << "\n";
  return true;
}

token_id Tokeniser::process_directive() {
  do {
    skip_ppline_extras();
    auto start = cur.it;
    if (!consume_identifier()) break;
    std::string_view directive_name(start, cur.it - start);

    if (directive_name == "include") {
      if (!process_include()) break;
      return token::pp_include;
    }
    if (directive_name == "define") {
      if (!process_define()) break;
      return token::pp_define;
    }
    // pp_undef
    skip_ppline();
    return token::pp_other_directive;
  } while (false);
  skip_ppline();
  return token::pp_error;
}
