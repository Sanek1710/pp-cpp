#pragma once

#include <ostream>

#include "Cursor.h"

class TokenPrinter {
 public:
  TokenPrinter(std::ostream& os) : os(os) {}

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
      if (token.id == token::line_comment ||
          token.id == token::multiline_comment)
        return "\033[38;5;22m";
      if (token.id == token::string_like_literal) return "\033[38;5;216m";
      if (keywords.count(text)) return "\033[38;5;176m";
      if (keywords2.count(text)) return "\033[38;5;75m";
      if (keywords3.count(text)) return "\033[38;5;37m";
      if (std::isdigit(text.front())) return "\033[38;5;193m";
      if (text.front() == '(' || text.front() == ')') return "\033[38;5;228m";
      if (std::ispunct(text.front())) return "\033[38;5;248m";
      if (token.id == token::identifier && *token.end == ':')
        return "\033[38;5;37m";
      if (token.id == token::identifier && *token.end == '(')
        return "\033[38;5;230m";

      return "\033[38;5;153m";
    };
    if (last_line != token.pos.line) {
      last_line = token.pos.line;
      os << "\033[38;5;240m" << std::setw(3) << (token.pos.line + 1) << "│ ";
    }
    os << clr() << text << "\033[0m";
  }

 private:
  unsigned last_line = -1;
  std::ostream& os;
};