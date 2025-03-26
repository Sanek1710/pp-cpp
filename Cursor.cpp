#include "Cursor.h"

#include <iomanip>
#include <iostream>
#include <sstream>

#include "StringToken.h"
#include "Token.h"
#include "helper.h"

using iterator = std::string_view::iterator;

std::ostream& operator<<(std::ostream& os, const Position& pos) {
  return os << "[" << std::setw(3) << pos.line    //
            << ":" << std::setw(2) << pos.column  //
            << "]";
}

std::ostream& operator<<(std::ostream& os, const Range& range) {
  return os << range.start_pos << " - " << range.end_pos;
}

void DefineImage::print(std::ostream& os, std::string_view src) const {
  os << "#define " << name.get_text(src);
  if (info.is_functional) {
    os << "(";
    if (info.nargs) {
      auto argit = args_begin();
      os << argit->get_text(src);
      for (++argit; argit != args_end(); ++argit) {
        os << ", " << argit->get_text(src);
      }
    }
    if (info.is_variadic) os << "...";
    os << ")";
  }
  os << " ";
  for (auto expIt = expansion_begin(); expIt != expansion_end(); ++expIt) {
    os << expIt->get_text(src);
  }
}

namespace {
void print_tag(std::ostream& os, Tag tag) {
  char kind_c = [tag]() {
    switch (tag::kindof(tag)) {
      case tag::Kind::raw:
        return 'r';
      case tag::Kind::extra:
        return 'e';
      case tag::Kind::grouped:
        return 'g';
      case tag::Kind::ppline:
        return 'p';
      case tag::Kind::aux:
        return 'a';
      default:
        return ' ';
        break;
    }
  }();
  std::stringstream ss;
  ss << std::setw(2) << kind_c  //
     << std::setw(2) << ctrl_str{static_cast<char>(tag & 0xFF)};
  os << ss.str();
}
}  // namespace

void Token::print(std::string_view src, std::ostream& os) const {
  os << 'y';
  print_tag(os, tag);
  os << range.start_pos << ": -> `" << ctrl_str{get_text(src)} << "`\n";
}

void StrToken::print(std::ostream& os) const {
  os << 'T';
  print_tag(os, tag);
  os << pos << ": `" << ctrl_str{text} << "`\n";
}
