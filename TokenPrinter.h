#pragma once

#include <iomanip>
#include <iostream>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "PositionMap.h"
#include "tkz/Position.h"
#include "tkz/Token.h"

// constexpr uint32_t mask3(Cursor::iterator it, Cursor::iterator end) {
//   uint32_t val = 0;
//   for (int i = 0; i < 4 && it != end; ++i, ++it) {
//     val = (val << 8) | *it;
//     if (*it == '"' || *it == '\'') return val;
//   }
//   return 0;
// }
// constexpr uint32_t mask3(std::string_view sv) {
//   return mask3(sv.begin(), sv.end());
// }

// constexpr auto m1 = mask3("u\"1231231213213");
// constexpr auto m2 = mask3("u\"12312sdvsdv13");

class TokenPrinter {
  static constexpr const char* magenta = "\033[38;5;176m";
  static constexpr const char* blue = "\033[38;5;75m";
  static constexpr const char* light_blue = "\033[38;5;153m";
  static constexpr const char* orange = "\033[38;5;216m";
  static constexpr const char* cyan = "\033[38;5;37m";
  static constexpr const char* green = "\033[38;5;22m";
  static constexpr const char* light_green = "\033[38;5;193m";
  static constexpr const char* yellow = "\033[38;5;228m";
  static constexpr const char* light_yellow = "\033[38;5;230m";
  static constexpr const char* white = "\033[38;5;248m";
  static constexpr const char* gray = "\033[38;5;240m";
  static constexpr const char* reset = "\033[0m";

  inline static const std::unordered_set<std::string_view> keywords = {
      "return", "if",    "using", "while",   "do",     "break", "else",   "for",
      "ifdef",  "endif", "else",  "include", "define", "case",  "switch",
  };
  inline static const std::unordered_set<std::string_view> type_keywords = {
      "auto",   "bool",      "char",   "class",    "const",       "constexpr",
      "inline", "int",       "public", "short",    "static_cast", "static",
      "struct", "template",  "this",   "unsigned", "void",        "false",
      "true",   "namespace", "assert", "private",  "mutable",     "sizeof",
  };
  inline static const std::unordered_set<std::string_view> std_types = {
      "std",           "string",  "size_t",       "string_view",
      "unordered_set", "ostream", "iterator",     "auto",
      "uint32_t",      "vector",  "shared_mutex", "uniqie_lock",
      "shared_lock",
  };

 public:
  TokenPrinter(std::ostream& os, bool print_lines = false)
      : os(os), print_lines(print_lines) {
    on_newline();
  }

  inline void print(Token tok, char peek = 0) {
    print(tok, peek, tok.start_pos);
  }

  inline void print(Token tok, char peek, Position translated_pos) {
    if (tok.tag == tag::identifier) {
      positions.push_back(tok.start_pos);
      translated_positions.push_back(translated_pos);
    }
    os << token_color(tok, peek);
    for (char c : tok.get_text()) {
      os << c;
      if (c == '\n') on_newline();
    }
    os << reset;
  }

  std::ostream& getos() const { return os; }

  static const char* token_color(Token tok, char peek) {
    const std::string_view text = tok.get_text();
    if (text.empty()) return "";
    if (tok.tag == tag::identifier) {
      if (keywords.count(text)) return magenta;
      if (type_keywords.count(text)) return blue;
      if (std_types.count(text)) return cyan;
      if (peek == ':') return cyan;
      if (peek == '(') return light_yellow;
      if (std::isupper(text.front())) return cyan;
      return light_blue;
    }
    if (tok.tag == tag::string_like_literal) return orange;
    if (tok.tag == tag::number) return light_green;
    if (tok.tag == tag::line_comment  //
        || tok.tag == tag::multiline_comment)
      return green;
    if (text == "(" || text == ")") return yellow;
    if (text == "{" || text == "}" || text == "#") return magenta;
    if (std::ispunct(text.front())) return white;
    return white;
  }

 private:
  unsigned nline = 0;
  bool print_lines = false;
  bool print_positions = true;
  std::vector<Position> positions;
  std::vector<Position> translated_positions;
  std::ostream& os;

  void on_newline() {
    if (print_positions && !positions.empty()) {
      print_positions_arr(positions);
      if (positions != translated_positions)
        print_positions_arr(translated_positions);
      positions.clear();
      translated_positions.clear();
    }
    if (print_lines) os << gray << std::setw(3) << (nline + 1) << "│ " << reset;
    ++nline;
  }

  void print_positions_arr(const std::vector<Position>& positions) {
    os << gray << "   │  ";
    unsigned last_col = 0;
    for (auto pos : positions) {
      const unsigned dcol = pos.column - last_col;
      os << std::setw(dcol) << pos.column + 1;
      last_col = pos.column;
    }
    os << "\n";
  }
};
