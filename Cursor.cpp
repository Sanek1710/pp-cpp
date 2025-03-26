#include "Cursor.h"

#include <iomanip>
#include <iostream>
#include <sstream>

#include "StringToken.h"
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

void Token::print(std::string_view src, std::ostream& os) const {
  std::stringstream ss;
  ss << std::setw(4) << std::hex << std::setfill('.') << tag;
  os << 't' << ss.str() << range.start_pos << ": -> `"
     << ctrl_str{get_text(src)} << "`\n";
}

void StrToken::print(std::ostream& os) const {
  std::stringstream ss;
  ss << std::setw(4) << std::hex << std::setfill('.') << tag;
  os << 'T' << ss.str() << pos << ": `" << ctrl_str{text} << "`\n";
}
