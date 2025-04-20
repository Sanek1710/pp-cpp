#pragma once

#include <charconv>

#include "tkz/Cursor.h"
#include "tkz/Position.h"
#include "tkz/Token.h"

class MacroExpansionTokeniser : private Tokeniser {
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
  MacroExpansionTokeniser(std::string_view src, Position start_pos = Position{})
      : Tokeniser(src, start_pos) {}

  using Tokeniser::eof;

  inline Token read_token() {
    Token token;
    token.start = cur.it;
    token.start_pos = cur.to_position();
    token.details = {0};
    token.tag = tag_next();
    if (token.tag == tag::arg) {
      cur.it = std::from_chars(cur.it, cur.end, token.details.index).ptr;
      token.tag = arg_tag_marker_to_arg_tag(*cur.it++);
    }
    token.size = cur.it - token.start;
#ifdef ENDPOS
    token.end_pos = cur.to_position();
#endif
    return token;
  }

 private:
  inline Tag tag_next() {
    if (cur.eof()) return tag::eof;
    if (*cur.it == '$') { /*5*/
      ++cur.it;
      if (*cur.it == '$') return tag::raw(*cur.it++);
      return tag::arg;
    }
    return Tokeniser::tag_ppnext();
  }
};
