#pragma once

#include <ostream>

#include "Cursor.h"

constexpr uint32_t mask3(Cursor::iterator it, Cursor::iterator end) {
  uint32_t val = 0;
  for (int i = 0; i < 4 && it != end; ++i, ++it) {
    val = (val << 8) | *it;
    if (*it == '"' || *it == '\'') return val;
  }
  return 0;
}
constexpr uint32_t mask3(std::string_view sv) {
  return mask3(sv.begin(), sv.end());
}

constexpr auto m1 = mask3("u\"1231231213213");
constexpr auto m2 = mask3("u\"12312sdvsdv13");

class TokenPrinter {
 public:
  TokenPrinter(std::ostream& os, bool print_lines = false)
      : os(os), print_lines(print_lines) {}

  inline void print(Token token, std::string_view src) {
    std::string_view text = token.get_text(src);
    auto clr = [text, &token, &src]() -> const char* {
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
      if (token.id == Token::line_comment ||
          token.id == Token::multiline_comment)
        return "\033[38;5;22m";
      if (token.id == Token::string_like_literal) return "\033[38;5;216m";
      if (keywords.count(text)) return "\033[38;5;176m";
      if (keywords2.count(text)) return "\033[38;5;75m";
      if (keywords3.count(text)) return "\033[38;5;37m";
      if (std::isdigit(text.front())) return "\033[38;5;193m";
      if (text.front() == '(' || text.front() == ')') return "\033[38;5;228m";
      if (std::ispunct(text.front())) return "\033[38;5;248m";
      if (token.id == Token::identifier && src[token.range.end] == ':')
        return "\033[38;5;37m";
      if (token.id == Token::identifier && src[token.range.end] == '(')
        return "\033[38;5;230m";

      return "\033[38;5;153m";
    };
    if (last_line != token.range.start_pos.line) {
      last_line = token.range.start_pos.line;
      if (print_lines)
        os << "\033[38;5;240m" << std::setw(3)
           << (token.range.start_pos.line + 1) << "│ ";
    }
    os << clr() << text << "\033[0m";
  }

  std::ostream& getos() const { return os; }

 private:
  unsigned last_line = -1;
  bool print_lines = false;
  std::ostream& os;
};