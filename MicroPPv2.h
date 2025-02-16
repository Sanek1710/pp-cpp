#pragma once

#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class MicroPPv2 {
 private:
  // Core state
  std::string_view code;
  std::string out;
  size_t head = 0;
  char c0 = '\0';  // Current character (for decisions)
  char c1 = '\0';  // Lookahead character

  // Line tracking
  unsigned line = 0;
  size_t line_start_offset = 0;
  bool at_line_start = true;

  struct Position {
    unsigned offset;
    unsigned line;
    unsigned column;
  };

  // Token identification - all checks are constexpr for compile-time evaluation
  static constexpr bool is_line_comment(char c0, char c1) {
    return c0 == '/' && c1 == '/';
  }
  static constexpr bool is_block_comment(char c0, char c1) {
    return c0 == '/' && c1 == '*';
  }
  static constexpr bool is_string_quote(char c) {
    return c == '"' || c == '\'';
  }
  static constexpr bool is_line_continuation(char c0, char c1) {
    return c0 == '\\' && c1 == '\n';
  }
  static constexpr bool is_directive(char c) { return c == '#'; }
  static constexpr bool is_identifier_start(char c) {
    return std::isalpha(c) || c == '_';
  }
  static constexpr bool is_identifier_part(char c) {
    return std::isalnum(c) || c == '_';
  }

  // Core parsing helpers
  bool safe_shift() {
    if (c0 == '\n') {
      ++line;
      line_start_offset = head;
      at_line_start = true;
    }
    c0 = c1;
    ++head;
    return (c1 = head < code.size() ? code[head] : '\0');
  }

  // Token tracking
  struct TokenTracker {
    Position start;
    size_t last_dump_offset;
    bool was_cat = false;
    unsigned n_noscreen_nl = 0;

    TokenTracker(const MicroPPv2* parser)
        : start(parser->get_pos()), last_dump_offset(parser->get_offset()) {}

    void on_newline() { ++n_noscreen_nl; }
    void on_cat() { was_cat = true; }
  };

  // Layout preservation
  void preserve_layout(const TokenTracker& tracker, size_t end_offset) {
    if (tracker.was_cat) {
      out.append(line - tracker.start.line - tracker.n_noscreen_nl, '\n');
      out.append(end_offset - line_start_offset, ' ');
    }
    dump(code.substr(tracker.start.offset, end_offset - tracker.start.offset),
         tracker.start.line, tracker.start.column);
  }

  // Token processing
  void skip_line_comment() {
    TokenTracker tracker(this);

    while (safe_shift()) {
      if (is_line_continuation(c0, c1)) {
        safe_shift();  // Skip the continuation
        continue;
      }
      if (c0 == '\n') break;
    }

    out.append(line - tracker.start.line, '\n');
    preserve_layout(tracker, get_offset());
  }

  void skip_block_comment(bool fill = true) {
    TokenTracker tracker(this);

    while (safe_shift()) {
      if (c0 == '*' && c1 == '/') {
        safe_shift();
        safe_shift();
        break;
      }
      if (c0 == '\n') tracker.on_newline();
    }

    if (fill) {
      out.append(line - tracker.start.line, '\n');
      out.append(
          get_offset() - std::max(tracker.start.offset, line_start_offset),
          ' ');
    }
    preserve_layout(tracker, get_offset());
  }

  void skip_string_literal() {
    char quote = c0;
    out += quote;

    TokenTracker tracker(this);

    while (safe_shift()) {
      if (c0 == quote) break;

      if (is_line_continuation(c0, c1)) {
        out.append(code.substr(tracker.last_dump_offset,
                               get_offset() - tracker.last_dump_offset));
        tracker.last_dump_offset = head + 1;
        tracker.on_cat();
        safe_shift();
        continue;
      }

      if (c0 == '\\') {
        safe_shift();
        continue;
      }

      if (c0 == '\n') tracker.on_newline();
    }
    safe_shift();

    size_t end_offset = get_offset();
    out.append(code.substr(tracker.last_dump_offset,
                           end_offset - tracker.last_dump_offset));
    preserve_layout(tracker, end_offset);
  }

  // Whitespace and comment skipping
  void skip_ws_and_ml_comments() {
    while (true) {
      if (is_block_comment(c0, c1)) {
        skip_block_comment(false);
        continue;
      }
      if (!std::isspace(c0) || c0 == '\n') break;
      if (!safe_shift()) break;
    }
  }

  // Identifier handling
  std::string_view get_identifier() {
    if (!is_identifier_start(c0)) return {};

    size_t start = head - 1;
    while (safe_shift() && is_identifier_part(c0)) {
    }

    return code.substr(start, (head - 1) - start);
  }

  // Token handling
  enum class TokenType {
    LineComment,    // //
    BlockComment,   // /* */
    StringLiteral,  // " or '
    Directive,      // #
    Identifier,     // [a-zA-Z_][a-zA-Z0-9_]*
    Operator,       // ## or #
  };

  struct TokenPattern {
    TokenType type;
    bool (*check)(char c0, char c1);
    void (MicroPPv2::*process)();
  };

  const TokenPattern token_patterns[3] = {
      {TokenType::LineComment, is_line_comment, &MicroPPv2::skip_line_comment},
      {TokenType::BlockComment, is_block_comment,
       &MicroPPv2::skip_multiline_comment},
      {TokenType::StringLiteral, is_string_quote,
       &MicroPPv2::skip_string_like_literal},
  };

  // Directive handling
  struct DirectivePattern {
    std::string_view name;
    void (MicroPPv2::*process)();
  };

  const DirectivePattern directive_patterns[2] = {
      {"define", &MicroPPv2::process_define},
      {"include", &MicroPPv2::process_include},
  };

  // Macro operator handling
  struct MacroOpPattern {
    bool (*check)(char c0, char c1);
    void (*process)(std::string& expansion);
  };

  static void process_concat(std::string& expansion) {
    while (!expansion.empty() && std::isspace(expansion.back())) {
      expansion.pop_back();
    }
  }

  static void process_stringize(std::string& expansion) {
    // Nothing to do here, just mark for stringizing
  }

  const MacroOpPattern macro_op_patterns[2] = {
      {is_concat_op, process_concat},
      {is_stringize_op, process_stringize},
  };

  // Core parsing helpers
  bool safe_shift() {
    if (c0 == '\n') {
      ++line;
      line_start_offset = head;
      at_line_start = true;
    }
    c0 = c1;
    ++head;
    return (c1 = head < code.size() ? code[head] : '\0');
  }

  void skip_ws_and_ml_comments() {
    while (true) {
      if (c0 == '/' && c1 == '*') {
        skip_multiline_comment(false);
        continue;
      }
      if (!std::isspace(c0) || c0 == '\n') break;
      if (!safe_shift()) break;
    }
  }

  std::string_view get_identifier() {
    if (!std::isalpha(c0) && c0 != '_') return {};

    size_t start = head - 1;
    while (safe_shift() && (std::isalnum(c0) || c0 == '_')) {
    }

    return code.substr(start, (head - 1) - start);
  }

  // Comment handling
  void skip_line_comment() {
    Position start = get_pos();

    while (safe_shift()) {
      if (c0 == '\\' && c1 == '\n') {
        safe_shift();
        continue;
      }
      if (c0 == '\n') break;
    }

    out.append(line - start.line, '\n');
    dump(code.substr(start.offset, get_offset() - start.offset), start.line,
         start.column);
  }

  void skip_multiline_comment(bool fill = true) {
    Position start = get_pos();

    while (safe_shift()) {
      if (c0 == '*' && c1 == '/') {
        safe_shift();
        safe_shift();
        break;
      }
    }

    if (fill) {
      out.append(line - start.line, '\n');
      out.append(get_offset() - std::max(start.offset, line_start_offset), ' ');
    }
    dump(code.substr(start.offset, get_offset() - start.offset), start.line,
         start.column);
  }

  // String literal handling
  void skip_string_like_literal() {
    char quote = c0;
    out += quote;

    Position start = get_pos();
    size_t last_dump_offset = get_offset();

    bool was_cat = false;
    unsigned n_noscreen_nl = 0;

    while (safe_shift()) {
      if (c0 == quote) break;
      if (c0 == '\\' && c1 == '\n') {
        out.append(
            code.substr(last_dump_offset, get_offset() - last_dump_offset));
        last_dump_offset = head + 1;
        was_cat = true;
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
    out.append(code.substr(last_dump_offset, end_offset - last_dump_offset));
    if (was_cat) {
      out.append(line - start.line - n_noscreen_nl, '\n');
      out.append(end_offset - line_start_offset, ' ');
    }
    dump(code.substr(start.offset, end_offset - start.offset), start.line,
         start.column);
  }

  // Macro handling
  struct MacroParam {
    std::string_view name;
    size_t position;
  };

  struct Macro {
    std::string_view name;
    std::vector<MacroParam> params;
    std::string expansion;
    bool has_varargs = false;
  };

  std::unordered_map<std::string_view, Macro> macros;

  void process_macro_params(Macro& macro) {
    safe_shift();  // Skip (

    while (c0 && c0 != ')') {
      skip_ws_and_ml_comments();

      if (c0 == '.') {
        if (c1 == '.' && head + 1 < code.size() && code[head + 1] == '.') {
          macro.has_varargs = true;
          safe_shift();
          safe_shift();
          safe_shift();
          skip_ws_and_ml_comments();
          if (c0 != ')') return;  // Error
          break;
        }
        return;  // Error
      }

      auto param_name = get_identifier();
      if (param_name.empty()) return;  // Error

      macro.params.push_back({param_name, macro.expansion.size()});

      skip_ws_and_ml_comments();
      if (c0 == ')') break;
      if (c0 != ',') return;  // Error
      safe_shift();
    }
    safe_shift();  // Skip )
  }

  void process_macro_expansion(Macro& macro) {
    std::string expansion;
    bool in_stringizing = false;

    while (c0 && c0 != '\n') {
      if (c0 == '\\' && c1 == '\n') {
        safe_shift();
        safe_shift();
        continue;
      }

      if (c0 == '/' && c1 == '/') {
        while (c0 && c0 != '\n') safe_shift();
        break;
      }

      if (c0 == '/' && c1 == '*') {
        skip_multiline_comment(false);
        expansion += ' ';
        continue;
      }

      if (c0 == '#') {
        if (c1 == '#') {
          while (!expansion.empty() && std::isspace(expansion.back())) {
            expansion.pop_back();
          }
          safe_shift();
          safe_shift();
          skip_ws_and_ml_comments();
          continue;
        }
        safe_shift();
        skip_ws_and_ml_comments();
        in_stringizing = true;
        continue;
      }

      if (std::isalpha(c0) || c0 == '_') {
        auto id = get_identifier();
        auto it =
            std::find_if(macro.params.begin(), macro.params.end(),
                         [id](const MacroParam& p) { return p.name == id; });

        if (it != macro.params.end()) {
          if (in_stringizing) {
            expansion += '"';
            expansion += '\\';
            expansion += std::to_string(it - macro.params.begin());
            expansion += '"';
          } else {
            expansion += '\\';
            expansion += std::to_string(it - macro.params.begin());
          }
        } else {
          expansion += id;
        }
        in_stringizing = false;
        continue;
      }

      if (!std::isspace(c0) || !in_stringizing) {
        expansion += c0;
      }
      safe_shift();
    }

    while (!expansion.empty() && std::isspace(expansion.back())) {
      expansion.pop_back();
    }
    macro.expansion = std::move(expansion);
  }

  void process_define() {
    skip_ws_and_ml_comments();

    auto name = get_identifier();
    if (name.empty()) return;  // Error

    Macro macro;
    macro.name = name;

    skip_ws_and_ml_comments();
    if (c0 == '(') {
      process_macro_params(macro);
      skip_ws_and_ml_comments();
    }

    process_macro_expansion(macro);
    macros[macro.name] = std::move(macro);
  }

  void process_include() {
    skip_ws_and_ml_comments();

    if (c0 != '<' && c0 != '"') return;  // Error
    char closing = c0 == '<' ? '>' : '"';

    Position start = get_pos();
    safe_shift();

    while (c0 && c0 != closing && c0 != '\n') safe_shift();

    if (c0 != closing) return;  // Error
    safe_shift();

    dump(code.substr(start.offset, get_offset() - start.offset), start.line,
         start.column);
  }

  void process_directive() {
    Position start = get_pos();
    safe_shift();
    skip_ws_and_ml_comments();

    auto directive = get_identifier();
    if (directive.empty()) return;

    bool found = false;
    for (const auto& pattern : directive_patterns) {
      if (directive == pattern.name) {
        (this->*pattern.process)();
        found = true;
        break;
      }
    }

    out.append(line - start.line, '\n');
  }

  Position get_pos() const {
    return {.offset = get_offset(),
            .line = line,
            .column = get_offset() - line_start_offset};
  }

  unsigned get_offset() const { return head - 1; }

  void dump(std::string_view text, unsigned line, unsigned col) {
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
    }(text.front());

    if (text.front() == '/') return;
    if (text.front() == '"') return;
    std::cerr << "[" << std::setw(2) << line << ":" << std::setw(2) << col
              << "]`" << clr << text << "\033[0m"
              << "`\n";
  }

 public:
  MicroPPv2(std::string_view input) : code(input) { out.reserve(input.size()); }

  std::string process() {
    if (code.empty()) return {};

    c1 = code[0];
    safe_shift();

    while (c0) {
      if (at_line_start && is_directive(c0)) {
        process_directive();
        continue;
      }

      bool token_found = false;
      for (const auto& pattern : token_patterns) {
        if (pattern.check(c0, c1)) {
          (this->*pattern.process)();
          token_found = true;
          break;
        }
      }
      if (token_found) continue;

      if (!std::isspace(c0)) at_line_start = false;
      out += c0;
      safe_shift();
    }

    return out;
  }
};