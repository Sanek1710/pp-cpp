#include "Cursor.h"

#include <iomanip>
#include <iostream>
#include <sstream>

#include "Token.h"
#include "helper.h"

using iterator = std::string_view::iterator;

std::ostream& operator<<(std::ostream& os, const Position& pos) {
  return os << "[" << std::setw(3) << pos.line    //
            << ":" << std::setw(2) << pos.column  //
            << "]";
}

void DefineImage::print(std::ostream& os) const {
  os << "#define " << name.get_text();
  if (info.is_functional) {
    os << "(";
    if (info.nargs) {
      auto argit = args_begin();
      os << argit->get_text();
      for (++argit; argit != args_end(); ++argit) {
        os << ", " << argit->get_text();
      }
    }
    if (info.is_variadic) os << "...";
    os << ")";
  }
  os << " ";
  for (auto expIt = expansion_begin(); expIt != expansion_end(); ++expIt) {
    os << expIt->get_text();
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

void Token::print(std::ostream& os) const {
  os << 'T';
  print_tag(os, tag);
  os << start_pos << ": -> `" << ctrl_str{get_text()} << "`\n";
}
