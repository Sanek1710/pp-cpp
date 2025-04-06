#pragma once

#include <charconv>

#include "Cursor.h"
#include "Position.h"
#include "Token.h"

class MacroExpansionTokeniser : private Tokeniser {
  DirectiveTokenImage tokenImage;

  inline static constexpr Tag arg_tag_marker_to_arg_tag(char op_tag) {
    switch (op_tag) {
      case tag::markerof(tag::arg_str):
        return tag::arg_str;
      case tag::markerof(tag::arg_raw):
        return tag::arg_raw;
      default:
        break;
    }
    return tag::arg;
  }

 public:
  MacroExpansionTokeniser(std::string_view src, Position start_pos = {0, 0}) : Tokeniser(src, tokenImage, start_pos) {}

  inline Token read_token() {
    Token token;
    token.start = cur.it;
    token.start_pos = cur.to_position();
    token.details = {0};
    token.tag = skip_next();
    if (token.tag == tag::arg) {
      cur.it = std::from_chars(cur.it, end, token.details.index).ptr;
      token.tag = arg_tag_marker_to_arg_tag(*cur.it++);
    }
    token.size = cur.it - token.start;
#ifdef ENDPOS
    token.end_pos = cur.to_position();
#endif
    return token;
  }

  inline Tag skip_next() {
    if (cur.it == end) return tag::eof;
    if (*cur.it == '$') { /*5*/
      ++cur.it;
      if (*cur.it == '$') return tag::raw(*cur.it++);
      return tag::arg;
    }
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

  inline bool eof() const { return Tokeniser::eof(); }
};
