#pragma once

#include <cctype>
#include <cstddef>
#include <locale>
#include <string_view>
#include <vector>

#include "Position.h"
#include "Token.h"
#include "TokenGroup.h"
#include "chars.h"

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

struct Cursor {
  positer it;
  // never derefer this! only for ptr arithmetics
  positer line_start_it = 0;
  unsigned nline = 0;
  bool clear_line = true;

  Cursor(positer begin, Position start_pos = {0, 0})
      : it(begin),
        nline(start_pos.line),
        line_start_it(it - start_pos.column) {}

  inline void enter() {
    line_start_it = it + 1;
    ++nline;
  }

  inline unsigned line() const { return nline; }
  inline unsigned column() const { return it - line_start_it; }

  inline Position to_position() const {
    return {.line = line(), .column = column()};
  }
};

// skip basic tokens assuming being on their start

inline void skip_blank(Cursor& cur, positer end) {
  ++cur.it;
  while (cur.it != end && is_space(*cur.it)) {
    if (*cur.it == '\n') return;
    ++cur.it;
  }
}

inline void skip_newline(Cursor& cur, positer end) {
  cur.enter();
  cur.clear_line = true;
  ++cur.it;
}

inline void skip_identifier(Cursor& cur, positer end) {
  cur.clear_line = false;
  for (++cur.it; cur.it != end; ++cur.it) {
    if (!is_word_char(*cur.it)) return;
  }
}
inline bool consume_identifier(Cursor& cur, positer end) {
  if (!is_word_start_char(*cur.it)) return false;
  skip_identifier(cur, end);
  return true;
}

inline void skip_number(Cursor& cur, positer end) {
  cur.clear_line = false;
  for (++cur.it; cur.it != end; ++cur.it) {
    // might be some complicated logic with [eEpP][+-] but meh
    // next char after them has to be number anyway, so idrc
    if (!is_num_char(*cur.it)) return;
  }
}

inline void skip_line_comment(Cursor& cur, positer end) {
  for (++ ++cur.it; cur.it != end; ++cur.it) {
    if (*cur.it == '\n') {
      if (*std::prev(cur.it) != '\\') return;
      cur.enter();
    }
  }
}
inline void skip_multiline_comment(Cursor& cur, positer end) {
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

inline void skip_string_literal(Cursor& cur, positer end, bool ppline) {
  cur.clear_line = false;
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
  consume_identifier(cur, end);
}

inline void skip_raw_string_literal(Cursor& cur, positer end, bool ppline) {
  const char quot = *cur.it;
  const positer delim_start = cur.it + 1;
  positer delim_end = cur.it + 1;
  while (delim_end != end) {
    if (*delim_end == '(') break;
    if (!std::isgraph(*delim_end)  //
        || *delim_end == ')' || *delim_end == '\\') {
      return skip_string_literal(cur, end, ppline);
    }
  }
  const size_t delim_size = delim_end - delim_start;
  cur.it = delim_end;
  for (++cur.it; cur.it != end; ++cur.it) {
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
  if (cur.it == end) return;
  consume_identifier(cur, end);
}

template <bool ppline>
inline bool consume_extras(Cursor& cur, positer end) {
  bool was_extra = false;
  while (true) {
    if (cur.it == end) return was_extra;
    switch (*cur.it) {
      case '\n':
        if constexpr (ppline) return was_extra;
        skip_newline(cur, end);
        break;

      case ' ':
      case '\t':
      case '\v' ... '\r':
        skip_blank(cur, end);
        break;

      case '/': {
        const positer it1 = cur.it + 1;
        if (it1 == end) return was_extra;
        if (*it1 == '/') {
          skip_line_comment(cur, end);
        } else if (*it1 == '*') {
          skip_multiline_comment(cur, end);
        } else {
          return was_extra;
        }
        break;
      }

      case '\\': {
        const positer it1 = cur.it + 1;
        while (it1 != end && is_space(*it1)) {
          if (*it1 == '\n') {
            cur.it = it1;  // '\\'
            cur.enter();   // '\n'
            ++cur.it;
            break;
          }
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

inline Tag tskip(Cursor& cur, size_t n, Tag tag) {
  cur.it += n;
  return tag;
}

inline Tag cond_pskip(Cursor& cur, bool cond, size_t n1, size_t n2) {
  return cond ? pskip(cur, n1) : pskip(cur, n2);
}

// main token processing unit
template <bool ppline>
inline Tag tag_common_template(Cursor& cur, positer end) {
  if (consume_extras<ppline>(cur, end)) return tag::space;

  const positer it0 = cur.it + 0;
  const positer it1 = cur.it + 1;
  const positer it2 = cur.it + 2;
  const positer it3 = cur.it + 3;

  const char c0 = it0 == end ? 0 : *it0;
  const char c1 = it1 == end ? 0 : *it1;
  const char c2 = it2 == end ? 0 : *it2;
  const char c3 = it3 == end ? 0 : *it3;
  
  cur.clear_line = false;
  switch (c0) {
    case 0:
      return tag::eof;

    case '0' ... '9':
      skip_number(cur, end);
      return tag::number;

    case 'a' ... 'z':
    case 'A' ... 'Z':
    case '_': {
      const positer start = cur.it;
      skip_identifier(cur, end);
      const size_t token_size = cur.it - start;
      bool is_raw = false;
      if (cur.it != end                           //
          && (*cur.it == '\'' || *cur.it == '"')  //
          && is_string_prefix(std::string_view{start, token_size}, is_raw)) {
        if (!is_raw) {
          skip_string_literal(cur, end, ppline);
        } else if (*cur.it == '\"') {
          skip_raw_string_literal(cur, end, ppline);
        }
        return tag::string_like_literal;
      }
      return tag::identifier;
    }

    case '\'':
    case '"':
      cur.clear_line = false;
      skip_string_literal(cur, end, ppline);
      return tag::string_like_literal;

    case '#':
      return c1 == '#' ? tskip(cur, 2, tag::pp_op_cat)
                       : tskip(cur, 1, tag::pp_op_str);

    case '%':
      if (c1 == ':')
        return (c2 == '%' && c3 == ':') ? tskip(cur, 2, tag::pp_op_cat)
                                        : tskip(cur, 1, tag::pp_op_str);
      return c1 == '>' || c1 == '=' ? pskip(cur, 2) : pskip(cur, 1);

    case '-':
      if (c1 == '>') return c2 == '*' ? pskip(cur, 3) : pskip(cur, 2);
      return c1 == '-' || c1 == '=' ? pskip(cur, 2) : pskip(cur, 1);

    case '.':
      if (c1 == '.')
        return c2 == '.' ? tskip(cur, 3, tag::ellipsis) : pskip(cur, 1);
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
      return tskip(cur, 1, tag::raw(*cur.it));
  }
}

inline Tag tag_common(Cursor& cur, positer end) {
  return tag_common_template<false>(cur, end);
}
inline Tag tag_ppcommon(Cursor& cur, positer end) {
  return tag_common_template<true>(cur, end);
}
inline bool skip_common_extras(Cursor& cur, positer end) {
  return consume_extras<false>(cur, end);
}
inline bool skip_ppcommon_extras(Cursor& cur, positer end) {
  return consume_extras<true>(cur, end);
}

class Tokeniser {
  using Tagger = Tag (Tokeniser::*)();

 public:
  static constexpr uint32_t max_src_size = ~uint32_t{};
  // try:
  DirectiveTokenImage& tokenImage;

  Tokeniser(std::string_view src, DirectiveTokenImage& tokenImage,
            Position start_pos = {0, 0})
      : src{src},
        cur{src.begin(), start_pos},
        end{src.end()},
        tokenImage(tokenImage) {
    if (src.size() > max_src_size) {
      src.remove_suffix(src.size() - max_src_size);
      end = src.end();
    }
  }

  inline Token read_token() { return read<&Tokeniser::skip_next>(); }
  inline Token read_pptoken() { return read<&Tokeniser::skip_ppnext>(); }

  std::string_view get_src() const { return src; }
  inline bool eof() const { return cur.it == end; }

 protected:
  std::string_view src;
  Cursor cur;
  positer end;

  template <Tagger TagF>
  inline Token read() {
    Token token;
    token.start = cur.it;
    token.start_pos = cur.to_position();
    token.details.index = 0;
    token.tag = (this->*TagF)();
    token.size = cur.it - token.start;
#ifdef ENDPOS
    token.end_pos = cur.to_position();
#endif
    return token;
  }

  // consume patterns without tokenisation
  inline Tag tag_include_string() {
    const char quot = *cur.it == '<' ? '>' :  //
                          (*cur.it == '"' ? '"' : 0);
    if (!quot) return false;
    for (++cur.it; cur.it != end; ++cur.it) {
      if (*cur.it == quot || *cur.it == '\n') break;
      if (*cur.it == '\n') {
        if (*std::prev(cur.it) != '\\') return tag::pp_include_string;
        cur.enter();
      }
    }
    if (*cur.it != '\n') ++cur.it;
    return tag::pp_include_string;
  }

  inline bool consume_ellipis() {
    if (*cur.it != '.'       //
        || end - cur.it < 3  //
        || cur.it[1] != '.' || cur.it[2] != '.')
      return false;
    cur.clear_line = false;
    cur.it += 3;
    return true;
  }

  inline bool consume_char(char c) {
    if (*cur.it != c) return false;
    ++cur.it;
    return true;
  }

  inline bool consume_identifier_token(Token& token) {
    token.start = cur.it;
    token.start_pos = cur.to_position();
    token.tag = tag::identifier;
    bool consumed = false;
    if (is_word_start_char(*cur.it)) {
      skip_identifier(cur, end);
      consumed = true;
    }
    token.size = cur.it - token.start;
#ifdef ENDPOS
    token.end_pos = cur.to_position();
#endif
    return consumed;
  }

  inline Tag skip_next() {
    if (cur.it == end) return tag::eof;
    if (cur.clear_line && *cur.it == '#') { /*5*/
      ++cur.it;
      return process_directive();
    }
    return tag_common(cur, end);
  }

  inline Tag skip_ppnext() { return tag_ppcommon(cur, end); }

  inline bool skip_extras() { return skip_common_extras(cur, end); }
  inline bool skip_ppextras() { return skip_ppcommon_extras(cur, end); }

  // used to skip preprocessor lines
  inline void skip_ppline() {
    while (cur.it != end && *cur.it != '\n') tag_ppcommon(cur, end);
  }

  inline bool process_include() {
    skip_ppcommon_extras(cur, end);
    if (cur.it == end || (*cur.it != '"' && *cur.it != '<')) return false;
    tokenImage.base_token = read<&Tokeniser::tag_include_string>();
    skip_ppline();
    return true;
  }

  inline bool process_define() {
    skip_ppextras();
    tokenImage.clear();
    if (!consume_identifier_token(tokenImage.base_token)) return false;

    // not actual loop, just for break outs
    while (consume_char('(')) {
      tokenImage.details.macroInfo.is_functional = true;
      skip_ppextras();
      if (consume_char(')')) break;
      while (true) {
        skip_ppextras();
        Token& last = tokenImage.tokens.emplace_back();
        ++tokenImage.details.macroInfo.nargs;
        const bool was_arg = consume_identifier_token(last);
        if (was_arg) skip_ppextras();

        if (consume_ellipis()) {
          tokenImage.details.macroInfo.is_variadic = true;
          skip_ppextras();
          if (!consume_char(')')) return false;
          break;
        }

        if (!was_arg) return false;
        if (consume_char(')')) break;
        if (!consume_char(',')) return false;
      }
      break;
    }
    skip_ppextras();
    while (cur.it != end && *cur.it != '\n') {
      tokenImage.tokens.push_back(read_pptoken());
    }

    return true;
  }
  inline bool process_undef() {
    skip_ppextras();
    auto start = cur.it;
    if (!consume_identifier_token(tokenImage.base_token)) return false;
    skip_ppline();
    return true;
  }
  inline Tag process_directive() {
    do {
      skip_ppextras();
      auto start = cur.it;
      if (!consume_identifier(cur, end)) break;
      std::string_view directive_name(start, cur.it - start);

      if (directive_name == "include") {
        if (!process_include()) break;
        return tag::pp_include;
      }
      if (directive_name == "define") {
        if (!process_define()) break;
        return tag::pp_define;
      }
      if (directive_name == "undef") {
        if (!process_undef()) break;
        return tag::pp_undef;
      }
      // pp_undef
      skip_ppline();
      return tag::pp_other_directive;
    } while (false);
    skip_ppline();
    return tag::pp_error;
  }
};
