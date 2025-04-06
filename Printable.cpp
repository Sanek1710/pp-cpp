#include "Cursor.h"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <unordered_map>

#include "Token.h"
#include "TokenGroup.h"
#include "helper.h"

using positer = std::string_view::iterator;

std::ostream& operator<<(std::ostream& os, const Position& pos) {
  return os << "[" << std::setw(3) << pos.line    //
            << ":" << std::setw(2) << pos.column  //
            << "]";
}

void DefineView::print(std::ostream& os) const {
  os << "#define " << base_token.get_text();
  if (info().is_functional) {
    os << "(";
    bool first = true;
    for (const auto arg : args_view()) {
      os << (first ? "" : ", ") << arg.get_text();
      first = false;
    }
    if (info().is_variadic) os << "...";
    os << ")";
  }
  os << " ";
  for (const auto exp_tok : expansion_view()) {
    os << exp_tok.get_text();
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
