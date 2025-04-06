#pragma once

#include <cctype>
#include <locale>
#include <string_view>
#include <vector>

#include "Position.h"
#include "Token.h"
#include "TokenGroup.h"
#include "chars.h"
#include "helper.h"

// Tokeniser protocol:
// skip: skips any tag
//   returns: tag
// consume: tries to consume specified tag
//   returns: consumed -> specified tag
//            else -> eof

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

// main token processing unit
template <bool ppline, bool extras_only = false>
inline Tag tag_common_template(Cursor& cur, positer end) {
  if (*cur.it == '\n') {
    skip_newline(cur, end);
    return tag::newline;
  }
  if (is_space(*cur.it)) { /*0*/
    skip_blank(cur, end);
    return tag::space;
  }
  if (is_digit(*cur.it)) {
    if constexpr (extras_only) return tag::number;
    skip_number(cur, end);
    return tag::number;
  }
  if (is_word_start_char(*cur.it)) { /*1*/
    if constexpr (extras_only) return tag::identifier;
    const positer start = cur.it;
    skip_identifier(cur, end);
    const size_t token_size = cur.it - start;
    bool is_raw = false;
    if (cur.it != end                           //
        && (*cur.it == '\'' || *cur.it == '"')  //
        && is_string_prefix(std::string_view{start, token_size}, is_raw)) {
      if (!is_raw) {
        skip_string_literal(cur, end, ppline);
        return tag::string_like_literal;
      } else if (*cur.it == '\"') {
        skip_raw_string_literal(cur, end, ppline);
        return tag::raw_string_literal;
      }
    }
    return tag::identifier;
  }

  if (*cur.it == '\'' || *cur.it == '"') { /*3*/
    if constexpr (extras_only) return tag::string_like_literal;
    cur.clear_line = false;
    skip_string_literal(cur, end, ppline);
    return tag::string_like_literal;
  }

  if (*cur.it == '/') { /*2*/
    const auto next_it = std::next(cur.it);
    if (next_it != end) {
      if (*next_it == '/') {
        skip_line_comment(cur, end);
        return tag::line_comment;
      }
      if (*next_it == '*') {
        skip_multiline_comment(cur, end);
        return tag::multiline_comment;
      }
    }
    if constexpr (extras_only) return tag::raw('/');
    cur.clear_line = false;
    return tag::raw(*cur.it++);
  }

  if (*cur.it == '\\') { /*4*/
    auto next_it = std::next(cur.it);
    while (next_it != end && is_space(*next_it)) {
      if (*next_it == '\n') {
        cur.it = next_it;  // '\\'
        cur.enter();       // '\n'
        ++cur.it;
        return tag::line_continuation;
      }
    }
    if constexpr (extras_only) return tag::raw('\\');
    cur.clear_line = false;
    return tag::raw(*cur.it++);
  }

  /*7*/
  if constexpr (extras_only) return tag::raw(*cur.it);

  const char op = *cur.it++;
  if (cur.it != end && cats_operator(op, *cur.it)) {
    ++cur.it;
    return tag::punct2;
  }
  return tag::raw(op);
}

template <bool ppline>
inline Tag tag_common_extras_template(Cursor& cur, positer end) {
  if (*cur.it == '\n') {
    cur.enter();
    cur.clear_line = true;
    ++cur.it;
    return tag::newline;
  }
  if (is_space(*cur.it)) { /*0*/
    skip_blank(cur, end);
    return tag::space;
  }

  if (is_word_char(*cur.it)) return tag::identifier;
  if (*cur.it == '\'' || *cur.it == '"') return tag::string_like_literal;

  if (*cur.it == '/') { /*2*/
    const auto next_it = std::next(cur.it);
    if (next_it != end) {
      if (*next_it == '/') {
        skip_line_comment(cur, end);
        return tag::line_comment;
      }
      if (*next_it == '*') {
        skip_multiline_comment(cur, end);
        return tag::multiline_comment;
      }
    }
    return tag::raw('/');
  }

  if (*cur.it == '\\') { /*4*/
    const auto next_it = std::next(cur.it);
    if (next_it != end && *next_it == '\n') {
      ++cur.it;     // '\\'
      cur.enter();  // '\n'
      ++cur.it;
      return tag::line_continuation;
    }
    return tag::raw('\\');
  }

  /*7*/
  return tag::raw(*cur.it);
}

inline Tag tag_common(Cursor& cur, positer end) {
  return tag_common_template<false, false>(cur, end);
}
inline Tag tag_ppcommon(Cursor& cur, positer end) {
  return tag_common_template<true, false>(cur, end);
}
inline Tag tag_common_extras(Cursor& cur, positer end) {
  return tag_common_template<false, true>(cur, end);
}
inline Tag tag_ppcommon_extras(Cursor& cur, positer end) {
  if (*cur.it == '\n') return tag::newline;
  return tag_common_template<true, true>(cur, end);
}

class Tokeniser {
  using TkzSkipper = Tag (Tokeniser::*)();

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

  template <TkzSkipper SkipF>
  inline Token read() {
    Token token;
    token.start = cur.it;
    token.start_pos = cur.to_position();
    token.details.index = 0;
    token.tag = (this->*SkipF)();
    token.size = cur.it - token.start;
#ifdef ENDPOS
    token.end_pos = cur.to_position();
#endif
    return token;
  }

  // consume patterns without tokenisation
  inline bool consume_include_string() {
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

  inline Tag skip_ppnext() {
    if (cur.it == end) return tag::eof;
    if (*cur.it == '#') { /*5*/
      ++cur.it;
      if (cur.it != end && *cur.it == '#') {
        ++cur.it;
        return tag::pp_op_cat;
      }
      return tag::pp_op_str;
    }
    return tag_ppcommon(cur, end);
  }

  inline void skip_extras() {
    while (cur.it != end) {
      Tag kind = tag_common_extras(cur, end);
      if (!tag::is_extra(kind)) break;
    }
  }

  inline void skip_ppline_extras() {
    while (cur.it != end) {
      Tag kind = tag_ppcommon_extras(cur, end);
      if (kind == tag::newline || !tag::is_extra(kind)) break;
    }
  }

  // used to skip preprocessor lines
  inline void skip_ppline() {
    while (cur.it != end && *cur.it != '\n') tag_ppcommon(cur, end);
  }

  inline bool process_include() {
    skip_ppline_extras();
    tokenImage.base_token.start = cur.it;
    tokenImage.base_token.start_pos = cur.to_position();
    if (!consume_include_string()) return false;
    tokenImage.base_token.size = cur.it - tokenImage.base_token.start;
#ifdef ENDPOS
    tokenImage.base_token.end_pos = cur.to_position();
#endif
    skip_ppline();
    return true;
  }

  inline bool process_define() {
    skip_ppline_extras();
    tokenImage.clear();
    if (!consume_identifier_token(tokenImage.base_token)) return false;

    // not actual loop, just for break outs
    while (consume_char('(')) {
      tokenImage.details.macroInfo.is_functional = true;
      skip_ppline_extras();
      if (consume_char(')')) break;
      while (true) {
        skip_ppline_extras();
        Token& last = tokenImage.tokens.emplace_back();
        ++tokenImage.details.macroInfo.nargs;
        const bool was_arg = consume_identifier_token(last);
        if (was_arg) skip_ppline_extras();

        if (consume_ellipis()) {
          tokenImage.details.macroInfo.is_variadic = true;
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
    while (cur.it != end && *cur.it != '\n') {
      tokenImage.tokens.push_back(read_pptoken());
    }

    return true;
  }
  inline bool process_undef() {
    skip_ppline_extras();
    auto start = cur.it;
    if (!consume_identifier_token(tokenImage.base_token)) return false;
    skip_ppline();
    return true;
  }
  inline Tag process_directive() {
    do {
      skip_ppline_extras();
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
