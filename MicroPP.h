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
      : code(code), cat_strings(cat_strings) {
    out.reserve(code.size());
  }
  // c0,c1 = '#_'
  void process_directive() {
    size_t start_line = nline;
    size_t start_offset = ahead - 1;
    size_t start_col = start_offset - last_line_offset;

    // out += c0;
    c0 = c1;
    for (++ahead; ahead < code.size(); ++ahead) {
      c1 = code[ahead];
      if (c0 == '\\') {
        if (c1 == '\n') {
          newline();
          safe_shift();
          c0 = c1;
          continue;
        }
        // out += c0;
        c0 = c1;
      } else if (c0 == '\n') {
        newline();
        break;
      } else if (c0 != '\\' && c1 == '\n')
        break;
      else if (c0 == '/' && c1 == '*') {
        skip_multiline_comment(true);
      } else {
        // out += c0;
      }
      c0 = c1;
    }
    newline();
    safe_shift();
    out.append(nline - start_line, '\n');
    // if (c0) out += c0;

    std::cerr << nline << "###\n";
  }

  std::string process() {
    if (code.empty()) return out;
    c0 = code[ahead];
    for (++ahead; ahead < code.size(); ++ahead) {
      c1 = code[ahead];  // look ahead
      if (c0 == '\n') {
        newline();
        out += c0;
      } else if (c0 == '/' && c1 == '/') {
        skip_line_comment();
        line_start = true;
      } else if (c0 == '/' && c1 == '*') {
        skip_multiline_comment();
      } else if (c0 == '"' || c0 == '\'') {
        skip_string_like_literal();
      } else if (c0 == '#' && line_start) {
        process_directive();
      } else {
        if (line_start && !std::isspace(c0)) line_start = false;
        out += c0;
      }
      c0 = c1;
    }
    if (c0) out += c0;

    return out;
  }

 private:
  std::string_view code;
  std::string out;
  size_t ahead = 0;
  size_t last_line_offset = 0;
  unsigned nline = 0;
  char c0 = '\0';
  char c1 = '\0';
  bool cat_strings = false;
  bool line_start = true;

  void safe_shift() {
    c0 = c1;
    c1 = ++ahead < code.size() ? code[ahead] : '\0';
  }

  bool is_comment_start() { return c0 == '/' && c1 == '/'; }
  bool is_multiline_comment_start() { return c0 == '/' && c1 == '*'; }

  // c0,c1 = "//"
  void skip_line_comment() {
    size_t start_line = nline;
    size_t start_offset = ahead - 1;
    size_t start_col = start_offset - last_line_offset;
    c0 = '\0';
    for (++ahead; ahead < code.size(); ++ahead) {
      c1 = code[ahead];
      if (c0 == '\n') newline();
      if (c0 != '\\' && c1 == '\n') break;
      c0 = c1;
    }
    newline();
    safe_shift();
    out.append(nline - start_line, '\n');
    dump(code.substr(start_offset, ahead - start_offset), start_line,
         start_col);
  }
  // c0,c1 = "/*"
  void skip_multiline_comment(bool in_macro = false) {
    size_t start_line = nline;
    size_t start_offset = ahead - 1;
    size_t start_col = start_offset - last_line_offset;
    c0 = '\0';
    for (++ahead; ahead < code.size(); ++ahead) {
      c1 = code[ahead];
      if (c0 == '\n')
        newline();
      else if (c0 == '*' && c1 == '/')
        break;
      c0 = c1;
    }
    safe_shift();
    if (in_macro) {
      out += ' ';
      return;
    }
    out.append(nline - start_line, '\n');
    out.append(ahead - std::max(start_offset, last_line_offset), ' ');
    dump(code.substr(start_offset, ahead - start_offset), start_line,
         start_col);
  }
  // c0,c1 = "'?"
  void skip_string_like_literal() {
    out += c0;
    char quot = c0;
    size_t start_line = nline;
    size_t start_offset = ahead - 1;
    size_t start_col = start_offset - last_line_offset;

    bool was_cat = false;
    unsigned n_noscreen_nl = 0;
    c0 = c1;
    for (++ahead; ahead < code.size(); ++ahead) {
      c1 = code[ahead];
      if (c0 == '\\') {
        if (c1 == '\n') {
          newline();
          if (cat_strings) {
            was_cat = true;
            safe_shift();
            c0 = c1;
            continue;
          }
        }
        out += c0;
        out += c1;
        safe_shift();
        c0 = c1;
        continue;
      } else if (c0 == '\n') {
        ++n_noscreen_nl;
        newline();
      } else if (c0 == quot) {
        out += c0;
        break;
      }
      out += c0;
      c0 = c1;
    }
    dump(code.substr(start_offset, ahead - start_offset), start_line,
         start_col);
    if (was_cat) {
      out.append(nline - start_line - n_noscreen_nl, '\n');
      out.append(ahead - 1 - std::max(start_offset, last_line_offset), ' ');
    }
  }

  void dump(std::string_view code, unsigned line, unsigned col) {
    std::cerr << "[" << line << ":" << col << "]```" << code << "```\n";
  }

  // Position save_position() { return Position{.line = }; }

  void newline() {
    ++nline;
    last_line_offset = ahead;
    line_start = true;
  }
};