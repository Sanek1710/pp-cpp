#pragma once

#include <cctype>
#include <cstddef>
#include <iomanip>
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

  // start with c0 at potential ws
  // end with c0 after all ws
  // no ws presence check
  // TODO: fix UB on EOF
  void skip_ws_and_ml_comments() {
    for (; ahead < code.size(); ++ahead) {
      c1 = code[ahead];
      if (c0 == '/' && c1 == '*') {
        skip_multiline_comment(true);
      } else if (!std::isspace(c0)) {
        break;
      }
      c0 = c1;
    }
  }

  std::string_view get_word() {
    size_t start_offset = ahead - 1;
    if (c0 != '_' && !std::isalpha(c0)) return {};
    c0 = c1;
    for (++ahead; ahead < code.size(); ++ahead) {
      c1 = code[ahead];
      if (c0 != '_' && !std::isalnum(c0)) break;
      c0 = c1;
    }
    return code.substr(start_offset, ahead - 1 - start_offset);
  }

  bool process_include_str() {
    char end = '\0';
    if (c0 == '"')
      end = '"';
    else if (c0 == '<')
      end = '>';
    else
      return false;
    size_t start_line = nline;
    size_t start_offset = ahead - 1;
    size_t start_col = start_offset - last_line_offset;
    c0 = c1;
    for (++ahead; ahead < code.size(); ++ahead) {
      c1 = code[ahead];
      if (c0 == '\n') {
        newline();
        return false;
      }
      if (c0 == end) break;
      c0 = c1;
    }
    safe_shift();
    auto include_name = code.substr(start_offset, ahead - 1 - start_offset);
    std::cerr << "#include " << include_name << "\n";
    dump(include_name, start_line, start_col);
    return true;
  }
  void process_include() {
    skip_ws_and_ml_comments();
    process_include_str();
  }

  void process_macro_expansion() {
    
  }

  void process_define() {
    skip_ws_and_ml_comments();
    auto macro_name = get_word();
    std::cerr << "#define " << macro_name << "\n";

    size_t start_line = nline;
    size_t start_offset = ahead - 1;
    size_t start_col = start_offset - last_line_offset;
    c0 = c1;
    for (++ahead; ahead < code.size(); ++ahead) {
      c1 = code[ahead];
      if (c0 == '\\') {
        if (c1 == '\n') {
          safe_shift();
          newline();
          c0 = c1;
          continue;
        }
        // out += c0;
        c0 = c1;
      } else if (c0 == '\n') {
        newline();
        break;
      } else if (c0 == '/' && c1 == '*') {
        skip_multiline_comment(true);
      } else {
        // out += c0;
      }
      c0 = c1;
    }
    safe_shift();
    newline();
    // out.append(nline - start_line, '\n');
    // if (c0) out += c0;
    out += code.substr(start_offset, ahead - start_offset);
  }

  // c0 == '#'
  std::string_view identify_directive() {
    // Skip whitespace and comments after #
    safe_shift();
    skip_ws_and_ml_comments();
    return get_word();
  }

  // c0,c1 = '#_'
  void process_directive() {
    // out += c0;
    std::string_view directive_name = identify_directive();
    if (directive_name == "define")
      process_define();
    else if (directive_name == "include")
      process_include();
  }

  bool check(char c0, char c1) { return this->c0 == c0 && this->c1 == c1; }

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
  void shift() {
    c0 = c1;
    c1 = code[++ahead];
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
    safe_shift();
    newline();
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
        // skip any screened chars
        if (c1 == '\n') {
          if (cat_strings) {
            was_cat = true;
          } else {
            out += c0;
            out += c1;
          }
          safe_shift();
          newline();
          c0 = c1;
          continue;
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
      out.append(ahead - std::max(start_offset, last_line_offset), ' ');
    }
  }

  void dump(std::string_view code, unsigned line, unsigned col) {
    auto clr = [](char c) {
      switch (c) {
        case '/':
          return "\033[32m";
        case '"':
          return "\033[33m";
        case '\'':
          return "\033[33m";
        default:
          return "\033[34m";
      }
    }(code.front());

    if (code.front() == '/') return;
    if (code.front() == '"') return;
    std::cerr << "[" << std::setw(2) << line << ":" << std::setw(2) << col
              << "]`" << clr << code << "\033[0m"
              << "`\n";
  }

  // Position save_position() { return Position{.line = }; }

  void newline() {
    ++nline;
    last_line_offset = ahead;
    line_start = true;
  }
};