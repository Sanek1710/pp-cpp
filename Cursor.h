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

struct Cursor {
  positer it;
  // never derefer this! only for ptr arithmetics
  positer line_start_it = 0;
  unsigned nline = 0;
  bool clear_line = true;

  Cursor(positer begin, Position start_pos = Position{})
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


class Tokeniser {
  using Tagger = Tag (Tokeniser::*)();

 public:
  static constexpr uint32_t max_src_size = ~uint32_t{};
  // TODO: move to handlers
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

  inline Token read_token() { return read<&Tokeniser::tag_next>(); }
  inline Token read_pptoken() { return read<&Tokeniser::tag_ppnext>(); }

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
