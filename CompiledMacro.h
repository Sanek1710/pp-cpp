#pragma once

#include <algorithm>
#include <charconv>
#include <string_view>

#include "tkz/Cursor.h"
#include "tkz/Token.h"
#include "tkz/TokenGroup.h"
#include "util/util.h"

struct MacroStamp {
  std::string_view expansion;
  MacroInfo info;
};

class CompiledMacro {
  static constexpr char variadic_marker = 'v';
  static constexpr char functional_marker = 'f';
  static constexpr char info_delimiter = ' ';

 public:
  CompiledMacro(const DefineView& macro);
  CompiledMacro(std::string_view raw) : compiled(raw) {}

  MacroStamp get_stamp() const;

  std::string&& take() && { return std::move(compiled); }
  std::string_view raw() const { return compiled; }

 private:
  std::string compiled;
};

using MacroMap = SegStringMap<CompiledMacro>;

inline std::ostream& operator<<(std::ostream& os, const MacroMap& macro_map) {
  for (const auto& [name, compiled_macro] : macro_map) {
    os << "  " << name << ": " << compiled_macro.get_stamp().expansion << "\n";
  }
  return os;
}

class CompiledMacroTokeniser : private Tokeniser {
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
  CompiledMacroTokeniser(MacroStamp macro_stamp,
                         Position start_pos = Position{})
      : Tokeniser(macro_stamp.expansion, start_pos) {}

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
