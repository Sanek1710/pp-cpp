#include "Cursor.h"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <unordered_map>

#include "Token.h"
#include "TokenGroup.h"
#include "util/helper.h"

namespace {

inline bool is_string_prefix(std::string_view str, bool& is_raw) {
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

inline void skip_blank(Cursor& cur) {
  ++cur.it;
  while (!cur.eof() && is_space(*cur.it)) {
    if (*cur.it == '\n') return;
    ++cur.it;
  }
}

inline void skip_newline(Cursor& cur) {
  cur.enter();
  cur.clear_line = true;
  ++cur.it;
}

inline void skip_identifier(Cursor& cur) {
  cur.clear_line = false;
  for (++cur.it; !cur.eof(); ++cur.it) {
    if (!is_word_char(*cur.it)) return;
  }
}
inline bool consume_identifier(Cursor& cur) {
  if (!is_word_start_char(*cur.it)) return false;
  skip_identifier(cur);
  return true;
}

inline void skip_number(Cursor& cur) {
  cur.clear_line = false;
  for (++cur.it; !cur.eof(); ++cur.it) {
    if (*cur.it == '+' || *cur.it == '-') {
      const char prev = *std::prev(cur.it);
      if (prev == 'e' || prev == 'E' || prev == 'p' || prev == 'P') continue;
      return;
    }
    if (!is_num_char(*cur.it)) return;
  }
}

inline void skip_line_comment(Cursor& cur) {
  for (++ ++cur.it; !cur.eof() && *cur.it != '\n'; ++cur.it)
    ;
}
inline void skip_multiline_comment(Cursor& cur) {
  ++ ++cur.it;
  if (cur.eof()) return;
  if (*cur.it == '\n') cur.enter();
  for (++cur.it; !cur.eof(); ++cur.it) {
    if (*cur.it == '/' && *std::prev(cur.it) == '*') {
      ++cur.it;
      return;
    }
    if (*cur.it == '\n') cur.enter();
  }
}

inline void skip_string_literal(Cursor& cur, bool ppline) {
  cur.clear_line = false;
  const char quot = *cur.it;
  bool escaped = false;
  for (++cur.it; !cur.eof(); ++cur.it) {
    if (ppline && *cur.it == '\n') return;
    if (*cur.it == '\\') {
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
  if (cur.eof()) return;
  consume_identifier(cur);
}

inline void skip_raw_string_literal(Cursor& cur, bool ppline) {
  const char quot = *cur.it;
  const positer delim_start = cur.it + 1;
  positer delim_end = cur.it + 1;
  while (delim_end != cur.end) {
    if (*delim_end == '(') break;
    if (!std::isgraph(*delim_end)  //
        || *delim_end == ')' || *delim_end == '\\') {
      return skip_string_literal(cur, ppline);
    }
    ++delim_end;
  }
  const size_t delim_size = delim_end - delim_start;
  cur.it = delim_end;
  if (cur.eof()) return;
  for (++cur.it; !cur.eof(); ++cur.it) {
    if (*cur.it == '\n') {
      cur.enter();
      if (ppline) return;
      continue;
    }
    if (*cur.it == quot                             //
        && *(cur.it - delim_size - 1) == '('        //
        && std::equal(cur.it - delim_size, cur.it,  //
                      delim_start, delim_end)) {
      ++cur.it;
      break;
    }
  }
  if (cur.eof()) return;
  consume_identifier(cur);
}

template <bool ppline>
inline bool consume_extras(Cursor& cur) {
  bool was_extra = false;
  while (true) {
    if (cur.eof()) return was_extra;
    switch (*cur.it) {
      case '\n':
        if constexpr (ppline) return was_extra;
        skip_newline(cur);
        break;

      case ' ':
      case '\t':
      case '\v' ... '\r':
        skip_blank(cur);
        break;

      case '/': {
        const positer it1 = cur.it + 1;
        if (it1 == cur.end) return was_extra;
        if (*it1 == '/') {
          skip_line_comment(cur);
        } else if (*it1 == '*') {
          skip_multiline_comment(cur);
        } else {
          return was_extra;
        }
        break;
      }

      default:
        return was_extra;
    }
    was_extra = true;
  }
}

inline Tag pskip(Cursor& cur, size_t n) {
  cur.it += n;
  // clang-format off
  switch (n) {
    case 1: return tag::raw(*(cur.it - 1));
    case 2: return tag::punct2;
    case 3: return tag::punct3;
    case 4: return tag::punct4;
    default: return tag::eof;
  }
  // clang-format on
}

inline Tag tag_skip(Cursor& cur, size_t n, Tag tag) {
  cur.it += n;
  return tag;
}

// main token processing unit
template <bool ppline>
inline Tag tag_common_template(Cursor& cur) {
  if (consume_extras<ppline>(cur)) return tag::space;

  const char c0 = cur.peek(0);
  const char c1 = cur.peek(1);
  const char c2 = cur.peek(2);
  const char c3 = cur.peek(3);

  cur.clear_line = false;
  switch (c0) {
    case 0:
      return tag::eof;

    case '\n':
      if constexpr (ppline) return tag::eof;
      return tag_skip(cur, 1, tag::newline);

    case '0' ... '9':
      skip_number(cur);
      return tag::number;

    case 'a' ... 'z':
    case 'A' ... 'Z':
    case '_': {
      const positer start = cur.it;
      skip_identifier(cur);
      const size_t token_size = cur.it - start;
      bool is_raw = false;
      if (!cur.eof()                              //
          && (*cur.it == '\'' || *cur.it == '"')  //
          && is_string_prefix(std::string_view{start, token_size}, is_raw)) {
        if (!is_raw) {
          skip_string_literal(cur, ppline);
        } else if (*cur.it == '\"') {
          skip_raw_string_literal(cur, ppline);
        }
        return tag::string_like_literal;
      }
      return tag::identifier;
    }

    case '\'':
    case '"':
      cur.clear_line = false;
      skip_string_literal(cur, ppline);
      return tag::string_like_literal;

    case '#':
      return c1 == '#' ? tag_skip(cur, 2, tag::pp_op_cat)
                       : tag_skip(cur, 1, tag::pp_op_str);

    case '%':
      if (c1 == ':')
        return (c2 == '%' && c3 == ':') ? tag_skip(cur, 4, tag::pp_op_cat)
                                        : tag_skip(cur, 2, tag::pp_op_str);
      return c1 == '>' || c1 == '=' ? pskip(cur, 2) : pskip(cur, 1);

    case '-':
      if (c1 == '>') return c2 == '*' ? pskip(cur, 3) : pskip(cur, 2);
      return c1 == '-' || c1 == '=' ? pskip(cur, 2) : pskip(cur, 1);

    case '.':
      if (c1 == '.')
        return c2 == '.' ? tag_skip(cur, 3, tag::ellipsis) : pskip(cur, 1);
      return c1 == '*' ? pskip(cur, 2) : pskip(cur, 1);

    case '<':
      if (c1 == '=') return c2 == '>' ? pskip(cur, 3) : pskip(cur, 2);
      if (c1 == '<') return c2 == '=' ? pskip(cur, 3) : pskip(cur, 2);
      return c1 == '%' || c1 == ':' ? pskip(cur, 2) : pskip(cur, 1);

    case '>':
      if (c1 == '>') return c2 == '=' ? pskip(cur, 3) : pskip(cur, 2);
      return c1 == '=' ? pskip(cur, 2) : pskip(cur, 1);

    // clang-format off
    case '&': return c1 == '&' || c1 == '=' ? pskip(cur, 2) : pskip(cur, 1);
    case '+': return c1 == '+' || c1 == '=' ? pskip(cur, 2) : pskip(cur, 1);
    case ':': return c1 == ':' || c1 == '>' ? pskip(cur, 2) : pskip(cur, 1);
    case '|': return c1 == '|' || c1 == '=' ? pskip(cur, 2) : pskip(cur, 1);
    
    case '!':
    case '*':
    case '/':
    case '=':
    case '^':
      return c1 == '=' ? pskip(cur, 2) : pskip(cur, 1);

    case '[': case ']': case '(': case ')': case '{':
    case '}': case ';': case '?': case ',': case '~':
      return pskip(cur, 1);
    // clang-format on

    // other
    default:
      return tag_skip(cur, 1, tag::raw(*cur.it));
  }
}

}  // namespace

Tag tag_common(Cursor& cur) { return tag_common_template<false>(cur); }
Tag tag_ppcommon(Cursor& cur) { return tag_common_template<true>(cur); }
bool skip_common_extras(Cursor& cur) { return consume_extras<false>(cur); }
bool skip_ppcommon_extras(Cursor& cur) { return consume_extras<true>(cur); }

Tag Tokeniser::tag_if_include_string() {
  const char quot = *cur.it == '<' ? '>' :  //
                        (*cur.it == '"' ? '"' : 0);
  if (!quot) return tag::empty;
  for (++cur.it; !cur.eof() && *cur.it != '\n'; ++cur.it) {
    if (*cur.it == quot) return tag_skip(cur, 1, tag::pp_include_string);
  }
  return tag::pp_include_string;
}

Tag Tokeniser::tag_if_identifier() {
  if (cur.eof() || !is_word_start_char(*cur.it)) return tag::empty;
  skip_identifier(cur);
  return tag::identifier;
}

Tag Tokeniser::tag_if_ellipis() {
  if (cur.size() < 3  //
      || cur.it[0] != '.' || cur.it[1] != '.' || cur.it[2] != '.')
    return tag::empty;
  return tag_skip(cur, 3, tag::ellipsis);
}

bool Tokeniser::consume_char(char c) {
  if (*cur.it != c) return false;
  ++cur.it;
  return true;
}

Tag Tokeniser::tag_next() {
  if (cur.eof()) return tag::eof;
  if (cur.clear_line && *cur.it == '#') { /*5*/
    ++cur.it;
    return process_directive();
  }
  return tag_common(cur);
}

Tag Tokeniser::tag_ppnext() { return tag_ppcommon(cur); }

bool Tokeniser::skip_extras() { return skip_common_extras(cur); }

bool Tokeniser::skip_ppextras() { return skip_ppcommon_extras(cur); }

inline void Tokeniser::process_ppline() {
  while (!cur.eof() && *cur.it != '\n') {
    mdirective_image.tokens.push_back(read_pptoken());
  }
}

bool Tokeniser::process_include() {
  skip_ppcommon_extras(cur);
  mdirective_image.base_token = read<&Tokeniser::tag_if_include_string>();
  if (mdirective_image.base_token.tag != tag::pp_include_string) return false;
  process_ppline();
  return true;
}

bool Tokeniser::process_define() {
  skip_ppextras();
  mdirective_image.base_token = read<&Tokeniser::tag_if_identifier>();
  if (mdirective_image.base_token.tag != tag::identifier) return false;

  // not actual loop, just for break outs
  while (consume_char('(')) {
    mdirective_image.details.macroInfo.is_functional = true;
    skip_ppextras();
    if (consume_char(')')) break;
    while (true) {
      skip_ppextras();
      ++mdirective_image.details.macroInfo.nargs;
      mdirective_image.tokens.push_back(read<&Tokeniser::tag_if_identifier>());
      if (mdirective_image.tokens.back().tag == tag::identifier)
        skip_ppextras();

      if (read<&Tokeniser::tag_if_ellipis>().tag == tag::ellipsis) {
        mdirective_image.details.macroInfo.is_variadic = true;
        skip_ppextras();
        if (!consume_char(')')) return false;
        break;
      }

      if (mdirective_image.tokens.back().tag != tag::identifier) return false;
      if (consume_char(')')) break;
      if (!consume_char(',')) return false;
    }
    break;
  }
  skip_ppextras();
  process_ppline();
  return true;
}

bool Tokeniser::process_undef() {
  skip_ppextras();
  auto start = cur.it;
  mdirective_image.base_token = read<&Tokeniser::tag_if_identifier>();
  if (mdirective_image.base_token.tag != tag::identifier) return false;
  process_ppline();
  return true;
}

Tag Tokeniser::process_directive() {
  mdirective_image.clear();
  do {
    skip_ppextras();
    mdirective_image.directive_token = read<&Tokeniser::tag_if_identifier>();
    if (mdirective_image.directive_token.tag != tag::identifier) break;
    const std::string_view directive_name{
        mdirective_image.directive_token.get_text()};

    if (directive_name == "include") {
      mdirective_image.mkind = DirectiveTokenImage::Kind::Include;
      if (!process_include()) break;
      return tag::pp_include;
    }
    if (directive_name == "define") {
      mdirective_image.mkind = DirectiveTokenImage::Kind::Define;
      if (!process_define()) break;
      return tag::pp_define;
    }
    if (directive_name == "undef") {
      mdirective_image.mkind = DirectiveTokenImage::Kind::Undef;
      if (!process_undef()) break;
      return tag::pp_undef;
    }
    mdirective_image.mkind = DirectiveTokenImage::Kind::Other;
    process_ppline();
    return tag::pp_other_directive;
  } while (false);
  mdirective_image.mkind = DirectiveTokenImage::Kind::Invalid;
  process_ppline();
  return tag::pp_invalid_directive;
}
