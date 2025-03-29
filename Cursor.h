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
  using iterator = std::string_view::iterator;

  iterator it;
  iterator line_start_it = 0;
  unsigned nline = 0;
  bool clear_line = true;

  Cursor(iterator begin) : it(begin), line_start_it(it) {}

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

class Tokeniser {
  using iterator = std::string_view::iterator;

 public:
  static constexpr uint32_t max_src_size = ~uint32_t{};
  // try:
  DefineImage defineImage;
  IncludeImage& includeImage = defineImage;
  UndefImage& undefImage = defineImage;

  Tokeniser(std::string_view src)  //
      : src{src}, cur{src.begin()}, end{src.end()} {
    if (src.size() > max_src_size) {
      src.remove_suffix(src.size() - max_src_size);
      end = src.end();
    }
  }

#define INLINEPPROC

  inline Token read_token() {
    Token token;
    token.start = cur.it;
    token.start_pos = cur.to_position();
    token.tag = skip_next();
    // TODO: maybe peek next and cat with previous
    // if we see line_continuation here
    token.size = cur.it - token.start;
    token.end_pos = cur.to_position();
    return token;
  }

  inline Token read_pptoken() {
    Token token;
    token.start = cur.it;
    token.start_pos = cur.to_position();
    token.tag = skip_ppnext();
    // TODO: maybe peek next and cat with previous
    // if we see line_continuation here
    token.size = cur.it - token.start;
    token.end_pos = cur.to_position();
    return token;
  }

  std::string_view get_src() const { return src; }

  inline bool eof() const { return cur.it == end; }

 protected:
  std::string_view src;
  Cursor cur;
  Cursor::iterator end;

  // skip basic tokens assuming being on their start
  inline void skip_identifier() {
    for (++cur.it; cur.it != end; ++cur.it) {
      if (!is_word_char(*cur.it)) return;
    }
  }

  inline void skip_number() {
    for (++cur.it; cur.it != end; ++cur.it) {
      // might be some complicated logic with [eEpP][+-] but meh
      // next char after them has to be number anyway, so idrc
      if (!is_num_char(*cur.it)) return;
    }
  }

  inline void skip_line_comment() {
    for (++ ++cur.it; cur.it != end; ++cur.it) {
      if (*cur.it == '\n') {
        if (*std::prev(cur.it) != '\\') return;
        cur.enter();
      }
    }
  }
  inline void skip_multiline_comment() {
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

  inline void skip_string_literal(bool ppline) {
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
    skip(3);
    return true;
  }

  inline bool consume_char(char c) {
    if (*cur.it != c) return false;
    ++cur.it;
    return true;
  }

  inline bool consume_identifier() {
    if (!is_word_start_char(*cur.it)) return false;
    skip_identifier();
    return true;
  }
  inline bool consume_identifier_token(Token& token) {
    token.start = cur.it;
    token.start_pos = cur.to_position();
    token.tag = tag::identifier;
    bool consumed = false;
    if (is_word_start_char(*cur.it)) {
      skip_identifier();
      consumed = true;
    }
    token.size = cur.it - token.start;
    token.end_pos = cur.to_position();
    return consumed;
  }

  inline void skip_ws() {
    ++cur.it;
    while (cur.it != end && is_space(*cur.it)) {
      if (*cur.it == '\n') return;
      ++cur.it;
    }
  }

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
    ++cur.it;     // '\\'
    cur.enter();  // '\n'
    ++cur.it;
  }

  inline Tag skip_next() {
    if (cur.it == end) return tag::eof;
    if (cur.clear_line && *cur.it == '#') { /*5*/
      ++cur.it;
      return process_directive();
    }
    return skip_common<false>();
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
    return skip_common<true>();
  }

  // main token processing unit
  template <bool ppline, bool extras_only = false>
  inline Tag skip_common() {
    if (*cur.it == '\n') {
      if constexpr (extras_only && ppline) return tag::newline;
      skip_newline();
      cur.clear_line = true;
      return tag::newline;
    }
    if (is_space(*cur.it)) { /*0*/
      skip_ws();
      return tag::space;
    }

    if (is_word_char(*cur.it)) { /*1*/
      if constexpr (extras_only) return tag::identifier;
      cur.clear_line = false;
      if (is_digit(*cur.it)) {
        skip_number();
        return tag::number;
      }
      const iterator start = cur.it;
      skip_identifier();
      const size_t token_size = cur.it - start;
      bool is_raw = false;
      if (cur.it != end                           //
          && (*cur.it == '\'' || *cur.it == '"')  //
          && is_string_prefix(std::string_view{start, token_size}, is_raw)) {
        if (!is_raw) {
          skip_string_literal(ppline);
          return tag::string_like_literal;
        } else if (*cur.it == '\"') {
          skip_string_literal(ppline);
          return tag::raw_string_literal;
        }
      }
      return tag::identifier;
    }

    if (*cur.it == '\'' || *cur.it == '"') { /*3*/
      if constexpr (extras_only) return tag::string_like_literal;
      cur.clear_line = false;
      skip_string_literal(ppline);
      return tag::string_like_literal;
    }

    if (*cur.it == '/') { /*2*/
      const auto next_it = std::next(cur.it);
      if (next_it != end) {
        if (*next_it == '/') {
          skip_line_comment();
          return tag::line_comment;
        }
        if (*next_it == '*') {
          skip_multiline_comment();
          return tag::multiline_comment;
        }
      }
      if constexpr (extras_only) return tag::raw('/');
      cur.clear_line = false;
      skip();
      return tag::raw('/');
    }

    if (*cur.it == '\\') { /*4*/
      const auto next_it = std::next(cur.it);
      if (next_it != end && *next_it == '\n') {
        skip_line_continuation();
        return tag::line_continuation;
      }
      if constexpr (extras_only) return tag::raw('\\');
      skip();
      return tag::raw('\\');
    }

    /*7*/
    if constexpr (extras_only) return tag::raw(*cur.it);
    return tag::raw(*cur.it++);
  }

  template <bool ppline>
  inline void skip_extras() {
    while (cur.it != end) {
      Tag kind = skip_common<false, /*extras_only*/ true>();
      if (!tag::is_extra(kind)) break;
    }
  }

  inline void skip_ppline_extras() {
    while (cur.it != end) {
      Tag kind = skip_common<true, /*extras_only*/ true>();
      if (kind == tag::newline || !tag::is_extra(kind)) break;
    }
  }

  // used to skip preprocessor lines
  inline void skip_ppline() {
    while (cur.it != end && *cur.it != '\n') skip_common<true>();
  }

  inline bool process_include() {
    skip_ppline_extras();
    includeImage.name.start = cur.it;
    includeImage.name.start_pos = cur.to_position();
    if (!consume_include_string()) return false;
    includeImage.name.size = cur.it - includeImage.name.start;
    includeImage.name.end_pos = cur.to_position();
    skip_ppline();
    return true;
  }
  inline bool process_define() {
    skip_ppline_extras();
    defineImage.clear();
    if (!consume_identifier_token(defineImage.name)) return false;

    // not actual loop, just for break outs
    while (consume_char('(')) {
      defineImage.info.is_functional = true;
      skip_ppline_extras();
      if (consume_char(')')) break;
      while (true) {
        skip_ppline_extras();
        Token& last = defineImage.tokens.emplace_back();
        ++defineImage.info.nargs;
        const bool was_arg = consume_identifier_token(last);
        if (was_arg) skip_ppline_extras();

        if (consume_ellipis()) {
          defineImage.info.is_variadic = true;
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
      defineImage.tokens.push_back(read_pptoken());
    }

    return true;
  }
  inline bool process_undef() {
    skip_ppline_extras();
    auto start = cur.it;
    if (!consume_identifier_token(undefImage.name)) return false;
    skip_ppline();
    return true;
  }
  inline Tag process_directive() {
    do {
      skip_ppline_extras();
      auto start = cur.it;
      if (!consume_identifier()) break;
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
