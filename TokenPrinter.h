#pragma once

#include <iomanip>
#include <ostream>
#include <string_view>
#include <unordered_set>

#include "Token.h"

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
 public:
  TokenPrinter(std::ostream& os, bool print_lines = false)
      : os(os), print_lines(print_lines) {
    on_newline();
  }

  inline void print(Token token) {
    std::string_view text = token.get_text();
    auto clr = [text, &token]() -> const char* {
      std::unordered_set<std::string_view> keywords = {
          "return",  "if",     "using", "while", "do",    "break",
          "else",    "for",    "#",     "ifdef", "endif", "else",
          "include", "define", "{",     "}",     "case",  "switch"};
      std::unordered_set<std::string_view> keywords2 = {
          "auto",        "bool",   "char",   "class",    "const",
          "constexpr",   "inline", "int",    "public",   "short",
          "static_cast", "static", "struct", "template", "this",
          "unsigned",    "void",   "false",  "true",     "namespace",
          "assert"};
      std::unordered_set<std::string_view> keywords3 = {
          "std",     "string",   "size_t", "string_view", "unordered_set",
          "ostream", "iterator", "auto",   "uint32_t",    "vector"};
      if (text.empty()) return "";
      if (token.tag == tag::line_comment || token.tag == tag::multiline_comment)
        return "\033[38;5;22m";
      if (token.tag == tag::string_like_literal) return "\033[38;5;216m";
      if (keywords.count(text)) return "\033[38;5;176m";
      if (keywords2.count(text)) return "\033[38;5;75m";
      if (keywords3.count(text)) return "\033[38;5;37m";
      if (std::isdigit(text.front())) return "\033[38;5;193m";
      if (text.front() == '(' || text.front() == ')') return "\033[38;5;228m";
      if (std::ispunct(text.front())) return "\033[38;5;248m";
      if (token.tag == tag::identifier && *token.end() == ':')
        return "\033[38;5;37m";
      if (token.tag == tag::identifier && *token.end() == '(')
        return "\033[38;5;230m";
      return "\033[38;5;153m";
    };

    os << clr();
    for (char c : text) {
      os << c;
      if (c == '\n') on_newline();
    }
    os << "\033[0m";
  }

  std::ostream& getos() const { return os; }

  void on_newline() {
    os << "\033[38;5;240m" << std::setw(3) << (nline + 1) << "│ ";
    ++nline;
  }

 private:
  unsigned nline = 0;
  bool print_lines = false;
  std::ostream& os;
};
