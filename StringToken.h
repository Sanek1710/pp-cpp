#pragma once

#include <cassert>
#include <ostream>

#include "Token.h"

struct StrToken {
  Tag tag;
  Position pos;
  std::string_view text;

  constexpr StrToken(Position pos, std::string_view text)
      : pos(pos), text(text), tag(tag::code) {}
  constexpr StrToken(const Token& token, std::string_view src)
      : pos(token.range.start_pos), text(token.get_text(src)), tag(token.tag) {}

  // inline constexpr bool extends_by(const StrToken& other) const {
  //   return text.end() == other.text.begin();
  // }

  // inline void extend(const StrToken& other) {
  //   assert(text.end() == other.text.begin());
  //   text = std::string_view{
  //       text.begin(), static_cast<size_t>(other.text.end() - text.begin())};
  // }
  void print(std::ostream& os) const;
};
