#pragma once

#include <cctype>
#include <cstddef>
#include <locale>
#include <string_view>
#include <vector>

#include "Position.h"
#include "Token.h"
#include "TokenGroup.h"
#include "util/chars.h"

struct Cursor {
  positer it;
  positer end;
  // never derefer this! only for ptr arithmetics
  positer line_start_it = 0;
  unsigned nline = 0;
  bool clear_line = true;

  Cursor(positer begin, positer end, Position start_pos = Position{})
      : it(begin),
        end(end),
        nline(start_pos.line),
        line_start_it(it - start_pos.column) {}

  inline void enter() {
    line_start_it = it + 1;
    ++nline;
  }

  inline char peek(size_t n = 0) { return it + n < end ? it[n] : 0; }

  inline unsigned line() const { return nline; }
  inline unsigned column() const { return it - line_start_it; }
  inline bool eof() const { return it == end; }

  inline unsigned size() const { return end - it; }

  inline Position to_position() const {
    return {.line = line(), .column = column()};
  }
};

class Tokeniser {
  using Tagger = Tag (Tokeniser::*)();

  // TODO: move to handlers
  DirectiveTokenImage mdirective_image;

 public:
  static constexpr uint32_t max_src_size = ~uint32_t{};

  Tokeniser(std::string_view src, Position start_pos = {0, 0})
      : cur{src.begin(), src.end(), start_pos} {
    if (src.size() > max_src_size) {
      src.remove_suffix(src.size() - max_src_size);
      cur.end = src.end();
    }
  }

  inline Token read_token() { return read<&Tokeniser::tag_next>(); }
  inline Token read_pptoken() { return read<&Tokeniser::tag_ppnext>(); }

  inline bool eof() const { return cur.eof(); }

  inline const DirectiveTokenImage& tokenImage() const {
    return mdirective_image;
  }

 protected:
  Cursor cur;

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

  // general taggers
  Tag tag_next();
  Tag tag_ppnext();
  bool skip_extras();
  bool skip_ppextras();

  // special taggers for convenience
  Tag tag_if_include_string();
  Tag tag_if_identifier();
  Tag tag_if_ellipis();
  bool consume_char(char c);

  // processing of directives
  void process_ppline();
  bool process_include();
  bool process_define();
  bool process_undef();
  Tag process_directive();
};
