#pragma once

#include <cctype>
#include <cstddef>
#include <iostream>
#include <string_view>

struct Position {
  unsigned line;
  unsigned col;
  unsigned offset;
};

class MicroParser {
 public:
  MicroParser(std::string_view code, bool cat_strings = true)
      : code(code), cat_strings(cat_strings) {}

  std::string process() {
    for (; head < code.size(); ++head) {
      c1 = code[head];
      if (c1 == '\n') {
        newline();
      }
      if (c0 == '/' && c1 == '/') {
        skip_line_comment();
        c1 = '\0';
      } else if (c0 == '/' && c1 == '*') {
        skip_multiline_comment();
        c1 = '\0';
      } else if (c1 == '"' || c1 == '\'') {
        skip_string_like_literal();
        c1 = '\0';
      } else {
        out += c1;
      }
      c0 = c1;
    }
    return out;
  }

 private:
  std::string_view code;
  std::string out;
  size_t head = 0;
  unsigned nline = 0;
  size_t last_line_offset = 0;
  char c0 = '\0';
  char c1 = '\0';
  bool cat_strings = true;

  void skip_line_comment() {
    out.pop_back();
    c0 = '\0';
    size_t start_line = nline;
    size_t start_offset = head - 1;
    size_t start_col = start_offset - last_line_offset;
    for (++head; head < code.size(); ++head) {
      c1 = code[head];
      if (c1 == '\n') newline();

      if (c0 != '\\' && c1 == '\n') {
        out.append(nline - start_line, '\n');
        dump(code.substr(start_offset, head - start_offset + 1), start_line,
             start_col);
        return;
      }
      c0 = c1;
    }
  }
  void skip_multiline_comment() {
    out.pop_back();
    c0 = '\0';
    size_t start_line = nline;
    size_t start_offset = head - 1;
    size_t start_col = start_offset - last_line_offset;
    for (++head; head < code.size(); ++head) {
      c1 = code[head];
      if (c1 == '\n') newline();

      if (c0 == '*' && c1 == '/') {
        out.append(nline - start_line, '\n');
        out.append(head + 1 - std::max(start_offset, last_line_offset), ' ');
        dump(code.substr(start_offset, head + 1 - start_offset), start_line,
             start_col);
        return;
      }
      c0 = c1;
    }
  }
  void skip_string_like_literal() {
    out += c1;
    char quot = code[head];
    char c0 = '\0';
    size_t start_line = nline;
    size_t start_offset = head;
    size_t start_col = start_offset - last_line_offset;
    bool was_cat = false;
    for (++head; head < code.size(); ++head) {
      char c1 = code[head];
      if (c1 == '\n') newline();
      if (cat_strings && c0 == '\\' && c1 == '\n') {
        was_cat = true;
        out.pop_back();
      } else {
        out += c1;
      }
      if (c0 != '\\' && c1 == quot) {
        if (was_cat) {
          out.append(nline - start_line, '\n');
          out.append(head + 1 - last_line_offset, ' ');
        }
        // dump(code.substr(start_offset, head - start_offset + 1), start_line,
        //      start_col);
        return;
      }
      c0 = c1;
    }
  }

  void dump(std::string_view code, unsigned line, unsigned col) {
    std::cerr << "[" << line << ":" << col << "]```" << code << "```\n";
  }

  // Position save_position() { return Position{.line = }; }

  void newline() {
    ++nline;
    last_line_offset = head + 1;
  }
};