#include "Cursor.h"

#include <iomanip>
#include <iostream>

#include "helper.h"

using iterator = std::string_view::iterator;

std::ostream& operator<<(std::ostream& os, const Position& pos) {
  return os << "[" << std::setw(3) << pos.line    //
            << ":" << std::setw(2) << pos.column  //
            << "]";
}

std::ostream& operator<<(std::ostream& os, const Range& range){
  return os << range.start_pos << " - " << range.end_pos;
}


void DefineImage::print(std::ostream& os, std::string_view src) const {
  std::cerr << "#define " << name.get_text(src);
  std::cerr << "(";
  // if (!args.empty()) {
  //   auto argit = args.begin();
  //   std::cerr << argit->get_text(src);
  //   for (++argit; argit != args.end(); ++argit) {
  //     std::cerr << ", " << argit->get_text(src);
  //   }
  // }
  std::cerr << ") ";
  // for (auto exp : expansion) {
  //   std::cerr << exp.get_text(src);
  // }
  std::cerr << "\n\n";
}

void Token::print(std::string_view src, std::ostream& os) const {
  os << id << range.start_pos << ": `" << ctrl_str{get_text(src)} << "`\n";
}
