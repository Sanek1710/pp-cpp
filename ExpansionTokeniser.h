#pragma once

#include <charconv>

#include "Cursor.h"

class MacroExpansionTokeniser : private Tokeniser {
  DirectiveTokenImage tokenImage;

 public:
  MacroExpansionTokeniser(std::string_view src) : Tokeniser(src, tokenImage) {}

  inline Token read_token() {
    Token token;
    token.start = cur.it;
    token.start_pos = cur.to_position();
    token.tag = skip_next();
    if (token.tag == tag::arg) {
      cur.it = std::from_chars(cur.it, end, token.details.index).ptr;
      cur.it++;
    }
    token.size = cur.it - token.start;
    token.end_pos = cur.to_position();
    return token;
  }

  inline Tag skip_next() {
    if (cur.it == end) return tag::eof;
    if (*cur.it == '$') { /*5*/
      ++cur.it;
      if (*cur.it == '$') return tag::raw(*cur.it++);
      return tag::arg;
    }
    return skip_common<false>(cur, end);
  }

  inline bool eof() const { return Tokeniser::eof(); }
};
