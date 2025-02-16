#pragma once

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <ostream>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

class CheckInner {
 public:
  CheckInner(const char* name) : name(name) {
    std::cerr << std::string(lvl, ' ') << "> " << name << "\n";
    ++lvl;
  }
  ~CheckInner() {
    --lvl;
    std::cerr << std::string(lvl, ' ') << "< " << name << "\n";
  }

  inline static size_t lvl = 0;
  const char* name;
};
#define CHECK_INNER  // const CheckInner __check_inner(__func__)

inline void pause() { std::cin.get(); }

struct MacroArgsView {
  std::vector<std::string_view> args;
  bool has_va_arg = false;
  // kinda optional, can be checked afterwards
  bool present = false;
  bool error = false;

  void print(std::ostream& os) {
    if (present) os << "  (  )\n";
    if (!present) os << "  [no args]\n";
    for (auto arg : args) {
      os << "  - " << arg << "\n";
    }
    if (has_va_arg) os << "    ...\n";
    if (error) os << "  [error]\n";
  }
};

struct MacroView {
  std::string_view name;
  std::string_view expansion;
  MacroArgsView args;

  void print(std::ostream& os) {
    os << "#define " << name << "\n";
    args.print(os);
    os << "  expand: `" << expansion << "`\n";
  }
};

class MicroParser {
  struct Position {
    unsigned offset;
    unsigned line;
    unsigned column;
  };

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
    while (true) {
      if (c0 == '/' && c1 == '*') {
        skip_multiline_comment(false);
        continue;
      }
      if (!std::isspace(c0) || c0 == '\n') break;
      safe_shift();
    }
  }

  std::string_view get_word() {
    size_t start_offset = get_offset();
    if (c0 != '_' && !std::isalpha(c0)) return {};
    while (safe_shift()) {
      // TODO: consider but would have to return string
      // cond: (c0 == '\\' && c1 == '\n') -> skip no append
      // we do need macroname as string anyway tho
      if (c0 != '_' && !std::isalnum(c0)) break;
    }
    size_t end_offset = get_offset();
    return code.substr(start_offset, end_offset - start_offset);
  }

  bool process_include_str() {
    char end = '\0';
    if (c0 == '"')
      end = '"';
    else if (c0 == '<')
      end = '>';
    else
      return false;
    Position start = get_pos();

    while (safe_shift()) {
      if (c0 == '\n') break;
      if (c0 == end) break;
    }
    safe_shift();

    unsigned end_offset = get_offset();
    auto include_name = code.substr(start.offset, end_offset - start.offset);
    dump(include_name, start.line, start.column);
    return true;
  }

  void process_include() {
    skip_ws_and_ml_comments();
    process_include_str();
    skip_ws_and_ml_comments();
  }

  void get_macro_args(MacroArgsView& macroArgs) {
    macroArgs.present = true;
    while (safe_shift()) {
      skip_ws_and_ml_comments();
      if (c0 == ')') break;
      macroArgs.args.push_back(get_word());
      skip_ws_and_ml_comments();
      if (c0 == '.') {
        macroArgs.has_va_arg = true;
        // TODO: hardcode 3 dots check?
        do {
          safe_shift();
        } while (c0 == '.');
        skip_ws_and_ml_comments();
        break;
      }
      if (macroArgs.args.back().empty()) macroArgs.error = true;
      if (c0 != ',') break;
    }
    if (c0 != ')')
      macroArgs.error = true;
    else
      safe_shift();
  }

  void process_macro_expansion(MacroView& macroView) {
    std::string compiled = "@000 ";
    bool line_start = true;
    bool cat_strings = true;

    while (c0) {
      if (c0 == '\\' && c1 == '\n') {
        safe_shift();
        safe_shift();
        continue;
      }
      if (c0 == '\n') {
        safe_shift();
        break;
      }
      if (c0 == '/' && c1 == '/') {
        skip_line_comment();
        break;
      }
      if (c0 == '/' && c1 == '*') {
        skip_multiline_comment();
        if (compiled.back() != ' ') compiled += ' ';
        continue;
      }
      if (c0 == '"' || c0 == '\'') {
        skip_string_like_literal();
        compiled += "\"###\"";
        continue;
      }
      if (c0 == '#') {
        if (c1 == '#') {
          safe_shift();
        } else {
          compiled += c0;
        }
        safe_shift();
        continue;
      }
      if (c0 == '#') {
        compiled += c0;
        safe_shift();
        continue;
      }
      if (c0 == '_' || std::isalpha(c0)) {
        // dumb dump
        auto pos = get_pos();
        auto word = get_word();
        const auto& args = macroView.args.args;
        auto argIt = (macroView.args.has_va_arg && word == "__VA_ARGS__")
                         ? std::prev(args.end())
                         : std::find(args.begin(), args.end(), word);
        if (argIt != args.end()) {
          std::cerr << "arg! " << *argIt << "\n";
          compiled += "\\";
          compiled += std::to_string(argIt - args.begin());
          if (!(c0 == '#' && c1 == '#')) {
            compiled += ' ';
          }
        } else {
          compiled += word;
        }
        dump(word, pos.line, pos.column);
        // check if argname
        continue;
      }
      if (std::isspace(c0)) {
        if (compiled.back() != ' ') compiled += ' ';
      } else {
        if (c0 == '\\') compiled += '\\';
        compiled += c0;
      }
      safe_shift();
    }
    while (std::isspace(compiled.back())) compiled.pop_back();

    std::cerr << compiled << "@\n";
  }

  std::string_view get_macro_expansion() {
    Position start = get_pos();
    while (true) {
      if (c0 != '\\' && c1 == '\n') break;
      if (c0 == '/' && c1 == '*') {
        skip_multiline_comment(false);
        continue;
      }
      safe_shift();
    }
    safe_shift();
    safe_shift();
    unsigned end_offset = get_offset();
    return code.substr(start.offset, end_offset - start.offset);
  }

  void process_define() {
    skip_ws_and_ml_comments();
    MacroView macroView;
    macroView.name = get_word();
    if (c0 == '(') {
      get_macro_args(macroView.args);
    }
    macroView.print(std::cerr);
    process_macro_expansion(macroView);
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
    CHECK_INNER;
    // append(c0);
    Position start = get_pos();
    State start_state = get_state();

    std::string_view directive_name = identify_directive();
    if (directive_name == "pragma") {
      std::cerr << directive_name << "\n";
    }
    if (directive_name == "define") {
      process_define();
      append(nline - start.line, '\n');
    } else if (directive_name == "include") {
      process_include();
      append(nline - start.line, '\n');
    } else {
      set_state(start_state);
      append(c0);
      safe_shift();
    }
  }

  bool check(char c0, char c1) { return this->c0 == c0 && this->c1 == c1; }

  std::string process() {
    if (code.empty()) return out;
    c1 = code[ahead];
    safe_shift();
    while (c0) {
      if (c0 == '/' && c1 == '/') {
        skip_line_comment();
        line_start = true;
        continue;
      }
      if (c0 == '/' && c1 == '*') {
        skip_multiline_comment();
        continue;
      }
      if (c0 == '"' || c0 == '\'') {
        skip_string_like_literal();
        continue;
      }
      if (c0 == '#' && line_start) {
        process_directive();
        continue;
      }
      if (line_start && !std::isspace(c0)) line_start = false;
      append(c0);
      safe_shift();
    }

    return out;
  }

 private:
  std::string_view code;
  std::string out;
  size_t ahead = 0;
  unsigned last_line_offset = 0;
  unsigned nline = 0;
  char c0 = '\0';
  char c1 = '\0';
  bool cat_strings = false;
  bool line_start = true;

  struct State {
    size_t ahead = 0;
    unsigned last_line_offset = 0;
    unsigned nline = 0;
  };

  State get_state() {
    return {
        .ahead = ahead, .last_line_offset = last_line_offset, .nline = nline};
  }

  void set_state(State state) {
    ahead = state.ahead;
    last_line_offset = state.last_line_offset;
    nline = state.nline;
    c0 = code[ahead - 1];
    c1 = code[ahead];
  }

  bool safe_shift() {
    if (c0 == '\n') newline();
    c0 = c1;
    ++ahead;
    return c1 = ahead < code.size() ? code[ahead] : '\0';
  }
  void shift() {
    c0 = c1;
    c1 = code[++ahead];
  }

  // c0,c1 = "//"
  void skip_line_comment() {
    CHECK_INNER;
    Position start = get_pos();

    while (safe_shift()) {
      if (c0 != '\\' && c1 == '\n') break;
    }
    safe_shift();
    safe_shift();

    append(nline - start.line, '\n');

    size_t end_offset = get_offset();
    dump(code.substr(start.offset, end_offset - start.offset), start.line,
         start.column);
  }
  // c0,c1 = "/*"
  void skip_multiline_comment(bool fill = true) {
    CHECK_INNER;
    Position start = get_pos();
    safe_shift();
    while (safe_shift()) {
      if (c0 == '*' && c1 == '/') break;
    }
    safe_shift();
    safe_shift();

    unsigned end_offset = get_offset();
    if (fill) {
      append(nline - start.line, '\n');
      append(end_offset - std::max(start.offset, last_line_offset), ' ');
    }
    dump(code.substr(start.offset, end_offset - start.offset), start.line,
         start.column);
  }
  // c0,c1 = "'?"
  void skip_string_like_literal() {
    CHECK_INNER;
    char quot = c0;

    Position start = get_pos();
    size_t last_dump_offset = get_offset();

    bool was_cat = false;
    unsigned n_noscreen_nl = 0;
    while (safe_shift()) {
      if (c0 == quot) break;
      if (c0 == '\\' && c1 == '\n') {
        if (cat_strings) {
          append(
              code.substr(last_dump_offset, get_offset() - last_dump_offset));
          last_dump_offset = ahead + 1;
          was_cat = true;
        }
        // TODO: this is actually too smart for preprocessor
        // as it doesnt care if '\' related to character other than \n
        // so gcc pp would just cat next case as "\"":
        // "\\
        // ""
        // but i think its more of an edge case or even UB
        // as source code before pp would basically be unreadable
        safe_shift();
        continue;
      }
      if (c0 == '\\') {
        safe_shift();
        continue;
      }
      if (c0 == '\n') ++n_noscreen_nl;
    }
    safe_shift();

    size_t end_offset = get_offset();
    append(code.substr(last_dump_offset, end_offset - last_dump_offset));
    if (was_cat) {
      append(nline - start.line - n_noscreen_nl, '\n');
      append(end_offset - last_line_offset, ' ');
    }
    dump(code.substr(start.offset, end_offset - start.offset), start.line,
         start.column);
  }

  void append(char c) { out += c; }
  void append(size_t n, char c) {
    if (n > 100000) {
      std::cout << n << "\n";
    }
    out.append(n, c);
  }
  void append(std::string_view sv) { out.append(sv); }

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

  Position get_pos() const {
    return {.offset = get_offset(),
            .line = nline,
            .column = get_offset() - last_line_offset};
  }

  unsigned get_offset() const { return ahead - 1; }

  void newline() {
    ++nline;
    last_line_offset = ahead;
    line_start = true;
  }
};