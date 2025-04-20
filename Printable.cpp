#include <iomanip>
#include <iostream>
#include <ostream>
#include <sstream>
#include <unordered_map>

#include "TokenPrinter.h"
#include "tkz/Cursor.h"
#include "tkz/Token.h"
#include "tkz/TokenGroup.h"
#include "util/ctrl.h"

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
  char kind_char = [tag]() {
    // clang-format off
    switch (tag::kindof(tag)) {
      case tag::Kind::raw:     return 'r';
      case tag::Kind::extra:   return 'e';
      case tag::Kind::grouped: return 'g';
      case tag::Kind::ppline:  return 'p';
      case tag::Kind::aux:     return 'a';
      default: break;
    }
    // clang-format on
    return ' ';
  }();
  std::stringstream ss;
  ss << std::setw(2) << kind_char  //
     << std::setw(2) << ctrl_str{tag::markerof(tag)};
  os << ss.str();
}

}  // namespace

void Token::print(std::ostream& os) const {
  os << 'T';
  print_tag(os, tag);
  os << start_pos << ": -> `" << ctrl_str{get_text()} << "`\n";
}

inline std::ostream& operator<<(std::ostream& os, Token tok) {
  os << 'T';
  print_tag(os, tok.tag);
  os << tok.start_pos << ": -> `" << ctrl_str{tok.get_text()} << "`\n";
  return os;
}

inline std::ostream& operator<<(std::ostream& os,
                                const IndexRange<std::vector<Token>>& tokens) {
  TokenPrinter printer{os, false};
  os << "|";
  for (const auto& token : tokens) {
    if (token.tag == tag::newline) {
      os << " \\n ";
    } else {
      printer.print(token);
    }
  }
  return os;
}