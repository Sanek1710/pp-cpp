#pragma once

#include <iostream>

#include "PositionMap.h"
#include "TokenPrinter.h"
#include "tkz/Cursor.h"
#include "tkz/Position.h"
#include "tkz/Token.h"

// inline void print_source(std::string_view src) {
//   Tokeniser tkz{src};
//   TokenPrinter printer{std::cerr, true};
//   while (!tkz.eof()) {
//     Token tok = tkz.read_token();
//     printer.print(tok, tkz.cursor().peek());
//   }
//   std::cerr << "\n";
// }

inline void print_source(std::string_view src) {
  Tokeniser tkz{src};
  TokenPrinter printer{std::cerr, true};
  unsigned last_line = 0;
  std::vector<Position> positions;
  while (!tkz.eof()) {
    Token tok = tkz.read_token();
    printer.print(tok, tkz.cursor().peek());
  }
  std::cerr << "\n";
}

inline void print_source(std::string_view src, const PositionMap& pos_map) {
  Tokeniser tkz{src};
  TokenPrinter printer{std::cerr, true};
  unsigned last_line = 0;
  std::vector<Position> positions;
  while (!tkz.eof()) {
    Token tok = tkz.read_token();
    printer.print(tok, tkz.cursor().peek());
  }
  std::cerr << "\n";
}