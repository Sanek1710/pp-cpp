#include <algorithm>
#include <cctype>
#include <iostream>
#include <iterator>
#include <string_view>
#include <vector>

#include "helper.h"

struct Position {
  unsigned offset;
  unsigned line;
  unsigned column;
};

bool isplain(char c) { return c != '\n' && std::isspace(c); }

class PPRawIterator {
 public:
  // Iterator traits
  using iterator_category = std::forward_iterator_tag;
  using value_type = char;
  using difference_type = std::ptrdiff_t;
  using pointer = const char*;
  using reference = const char&;
  using iterator = std::string_view::iterator;

 private:
  iterator begin;
  iterator end;
  iterator last;
  iterator it;
  unsigned line_start_offset = 0;
  unsigned nline = 0;
  // cache

 public:
  PPRawIterator(std::string_view str)
      : begin(str.begin()), end(str.end()), it(begin) {
    last = begin == end ? end : std::prev(end);
  }

  PPRawIterator(iterator begin, iterator end, iterator current)
      : begin(begin), end(end), it(current) {}

  reference operator*() const { return *it; }

  inline char next() const { return *std::next(it); }

  inline unsigned offset() const { return it - begin; }
  inline unsigned line() const { return nline; }
  inline unsigned column() const { return offset() - line_start_offset; }
  inline unsigned line_offset() const { return line_start_offset; }

  Position position() const {
    return Position{.offset = offset(), .line = line(), .column = column()};
  }
  std::string_view substr(unsigned from_offset) const {
    return {begin + from_offset, offset() - from_offset};
  }

  inline PPRawIterator& operator++() {
    if (*it++ == '\n') {
      line_start_offset = offset();
      ++nline;
    }
    return *this;
  }

  bool ltlast() const { return it < last; }
  bool ltend() const { return it < end; }

  difference_type nleft() const { return end - it; }
};

// Type aliases
using PPIter = PPRawIterator;

// Helper functions
struct MacroView {
  std::string_view name;
  std::string_view expansion;
  std::vector<std::string_view> args;
  bool is_functional = false;
  bool has_va_arg = false;

  void print(std::ostream& os) {
    os << "#define " << name;
    if (is_functional) {
      bool first = true;
      os << "(";
      for (auto arg : args) {
        os << (first ? "" : ", ") << arg;
        first = false;
      }
      if (has_va_arg) os << "...";
      os << ")";
    }
    os << " " << expansion << "\n";
  }
};

class Dumper {
 public:
  void dump_include(std::string_view include) {
    // std::cerr << "#include " << include << "\n";
  }
  void dump_macro(MacroView&& macro_view) {  //
    // macro_view.print(std::cerr);
  }
  void dump_ref(std::string_view ref) {  //
    // std::cerr << "> " << ref << "\n";
  }
  void dump_code() {}
};

Dumper dumper;

void skip_ws(PPIter& it) {
  for (; it.ltlast(); ++it) {
    if (std::isspace(*it)) continue;
    if (*it != '\\' || it.next() != '\n') return;
    ++it;
  }
  if (!it.ltend()) return;
  if (std::isspace(*it)) ++it;
}

void skip_plain_ws(PPIter& it) {
  for (; it.ltlast(); ++it) {
    if (isplain(*it)) continue;
    if (*it != '\\' || it.next() != '\n') return;
    ++it;
  }
  if (!it.ltend()) return;
  if (isplain(*it)) ++it;
}

void skip_line(PPIter& it) {
  for (; it.ltlast(); ++it) {
    if (*it == '\n') return;
    if (*it == '\\' && it.next() == '\n') ++it;
  }
  // handle last
  if (!it.ltend()) return;
  if (*it != '\n') ++it;
}

void skip_line_comment(PPIter& it) { skip_line(it); }

void skip_multiline_comment(PPIter& it) {
  ++it;  // skip '/'
  for (++it; it.ltlast(); ++it) {
    if (*it == '*' && it.next() == '/') break;
  }
  if (it.ltlast()) ++it;  // skip '*'
  ++it;                   // skip '/' or last
}

void skip_string_like_literal(PPIter& it, bool ppline = false) {
  const char quot = *it;
  bool escaped = false;
  for (++it; it.ltlast(); ++it) {
    if (ppline && *it == '\n') return;
    if (*it == '\\' && it.next() == '\n') {
      ++it;
      continue;
    }
    if (escaped) {
      escaped = false;
      continue;
    }
    if (*it == quot) break;
    if (*it == '\\') escaped = true;
  }
  if (!it.ltend()) return;
  ++it;  // skip quot or last
}

void skip_word_like(PPIter& it) {
  for (++it; it.ltend(); ++it) {
    if (!std::isalnum(*it) && *it != '_') return;
  }
}
bool consume_identifier(PPIter& it) {
  if (!std::isalpha(*it) && *it != '_') return false;
  skip_word_like(it);
  return true;
}
bool consume_number(PPIter& it) {
  if (!std::isdigit(*it)) return false;
  skip_word_like(it);
  return true;
}

std::string_view get_identifier(PPIter& it) {
  unsigned start = it.offset();
  consume_identifier(it);
  return it.substr(start);
}

void skip_ppline_extras(PPIter& it) {
  while (it.ltlast()) {
    if (*it == '/' && it.next() == '*') {
      skip_multiline_comment(it);
      continue;
    }
    if (*it == '\\' && it.next() == '\n')
      ++it;
    else if (!isplain(*it))
      return;
    ++it;
  }
  if (!it.ltend()) return;
  if (*it != '\n') ++it;
}

void skip_ppline(PPIter& it) {
  while (it.ltlast()) {
    if (*it == '\n') return;
    if (*it == '/' && it.next() == '*') {
      skip_multiline_comment(it);
      continue;
    }
    if (*it == '/' && it.next() == '/') {
      skip_line_comment(it);
      return;
    }
    if (*it == '"' || it.next() == '\'') {
      skip_string_like_literal(it, true);
      continue;
    }
    if (std::isalpha(*it) || *it == '_') {
      auto identifier = get_identifier(it);
      dumper.dump_ref(identifier);
      continue;
    }
    if (std::isdigit(*it)) {
      skip_word_like(it);
      continue;
    }

    if (*it == '\\' && it.next() == '\n') ++it;
    ++it;
  }
  if (*it != '\n') ++it;
}

std::string_view get_pp_line(PPIter& it) {
  const unsigned start = it.offset();
  skip_ppline(it);
  return it.substr(start);
}

// INCLUDE PROCESSING
void skip_include_string(PPIter& it) {
  const char quot = *it == '<' ? '>' :  //
                        (*it == '"' ? '"' : 0);
  if (!quot) return;
  for (++it; it.ltlast(); ++it) {
    if (*it == quot || *it == '\n') break;
    if (*it == '\\' && it.next() == '\n') ++it;
  }
  if (*it != '\n') ++it;
}

std::string_view get_include_string(PPIter& it) {
  unsigned start = it.offset();
  skip_include_string(it);
  return it.substr(start);
}

bool process_include(PPIter& it) {
  skip_ppline_extras(it);
  auto include_string = get_include_string(it);
  skip_ppline(it);
  if (include_string.empty()) return false;
  dumper.dump_include(include_string);
  return true;
}

// DEFINE PROCESSING

bool get_macro_args(PPIter& it, MacroView& macro) {
  macro.is_functional = *it == '(';
  if (!macro.is_functional) return true;

  for (++it; it.ltend(); ++it) {
    skip_ppline_extras(it);
    if (!it.ltend()) return false;

    if (*it == ')') break;
    macro.args.push_back(get_identifier(it));
    if (!it.ltend()) return false;

    skip_ppline_extras(it);
    if (!it.ltend()) return false;

    // va args?
    if (*it == '.') {
      macro.has_va_arg = true;
      if (it.nleft() < 4) return false;
      if (*++it != '.') return false;
      if (*++it != '.') return false;
      skip_ppline_extras(++it);
      break;
    }
    if (macro.args.back().empty()) return false;
    if (*it != ',') break;
  }

  if (!it.ltend()) return false;
  if (*it != ')') return false;
  ++it;
  return true;
}

bool process_define(PPIter& it) {
  do {
    skip_ppline_extras(it);
    if (!it.ltend()) return false;
    MacroView macroView;
    macroView.name = get_identifier(it);
    if (!it.ltend()) return !macroView.name.empty();

    if (macroView.name.empty()) break;

    if (!get_macro_args(it, macroView)) break;
    macroView.expansion = get_pp_line(it);
    dumper.dump_macro(std::move(macroView));
    return true;
  } while (false);
  skip_ppline(it);
  return false;
}

bool process_directive(PPIter& it) {
  ++it;  // skip '#'
  if (!it.ltend()) return false;

  skip_ppline_extras(it);
  if (!it.ltend()) return false;

  std::string_view directive = get_identifier(it);
  if (!it.ltend()) return false;

  if (directive == "define") return process_define(it);
  if (directive == "include") return process_include(it);

  skip_ppline(it);
  return false;
}

static constexpr bool is_string_prefix(std::string_view str) {
  const char* str_prefixes[] = {
      "R", "L", "LR", "u8", "u8R", "u", "uR", "U", "UR",
  };
  bool res = false;
  for (auto str_prefix : str_prefixes) res |= str == str_prefix;
  return res;
}

bool process_code(PPIter& it, std::string& out) {
#define DUMP_LOGIC(stmt) stmt
  out.clear();
  if (!it.ltend()) return true;

  bool line_start = true;
  unsigned last_dump_offset = 0;
  auto dump = [&out, &it, &last_dump_offset]() {
    DUMP_LOGIC(out.append(it.substr(last_dump_offset));)
    DUMP_LOGIC(last_dump_offset = it.offset();)
  };

  while (it.ltlast()) {
    if (std::isspace(*it)) {
      if (*it == '\n') line_start = true;
      ++it;
      continue;
    }

    if (line_start && *it == '#') {
      dump();
      const unsigned line = it.line();
      process_directive(it);
      DUMP_LOGIC(out.append(it.line() - line, '\n');)
      last_dump_offset = it.offset();
      continue;
    }

    if (std::isalpha(*it) || *it == '_') {
      line_start = false;
      auto identifier = get_identifier(it);
      static const char* str_prefixes[] = {
          "R", "L", "LR", "u8", "u8R", "u", "uR", "U", "UR",
      };
      if (it.ltlast() && (*it == '\'' || *it == '"')) {
        bool is_str_prefix = false;
        for (unsigned i = 0; i < sizeof(str_prefixes); ++i) {
          if (identifier == str_prefixes[i]) {
            is_str_prefix = true;
            break;
          }
        }
        if (is_str_prefix) {
          skip_string_like_literal(it);
          continue;
        }
      }

      dump();
      continue;
    }
    if (std::isdigit(*it)) {
      skip_word_like(it);
      continue;
    }

    if (*it == '/' && it.next() == '/') {
      dump();
      const unsigned line = it.line();
      skip_line_comment(it);
      DUMP_LOGIC(out.append(it.line() - line, '\n');)
      last_dump_offset = it.offset();
      continue;
    }

    if (*it == '/' && it.next() == '*') {
      dump();
      const unsigned start_line = it.line();
      const unsigned start_offset = it.offset();
      skip_multiline_comment(it);
      DUMP_LOGIC(out.append(it.line() - start_line, '\n');)
      out.append(it.offset() - std::max(it.line_offset(), start_offset), ' ');
      last_dump_offset = it.offset();
      // printit("\033[34m");
      // printit(it.substr(start_offset));
      // printit("\033[0m");
      continue;
    }

    if (*it == '\'' || *it == '"') {
      line_start = false;
      const unsigned start = it.offset();
      skip_string_like_literal(it);
      // printit("\033[33m");
      // printit(it.substr(start));
      // printit("\033[0m");
      continue;
    }

    if (*it == '\\' && it.next() == '\n') {
      ++it;
      continue;
    }

    line_start = false;
    ++it;
  }
  dump();
  return true;
}

int main(int argc, char* argv[]) {
  timeit;
  checkin;
  //~8.9 Mb
  // std::string src = read_file(ROOT "/pp.test/test.cpp");
  std::string src = read_file(ROOT "/sqliteall.c");
  printit(src.size());
  std::string out;

  if (true) {
    timeit;
    PPIter it{src};
    process_code(it, out);
    printit(it.nleft());
  }
  write_file(ROOT "/out.pp.c", out);
  // return true;
  size_t summer = 0;
  // if (true) {
  //   //~8.9 Gb benchmark
  //   stimeit("iterate code");
  //   std::string out;
  //   PPIter it(src);
  //   printit(it.ltlast());
  //   printit(it.ltend());
  //   printit(it.nleft());
  //   process_code(it, out);
  //   return 0;
  // }

  if (true) {
    //~8.9 Gb benchmark
    stimeit("process_code 100 times");
    std::string out;
    for (int i = 0; i < 100; ++i) {
      PPIter it(src);
      process_code(it, out);
    }
    summer += out.size();
  }

  if (true) {
    //~8.9 Gb benchmark
    stimeit("just reading");
    std::string out;
    for (int i = 0; i < 100; ++i) {
      for (auto c : src) summer += c;
    }
  }

  if (true) {
    //~8.9 Gb benchmark
    stimeit("for: raw_shift");
    std::string out;
    for (int i = 0; i < 100; ++i) {
      for (PPIter it(src); it.ltend(); ++it) {
        summer += *it;
      }
    }
  }

  if (true) {
    //~8.9 Gb benchmark
    stimeit("for: shift");
    std::string out;
    for (int i = 0; i < 100; ++i) {
      for (PPIter it(src); it.ltend(); ++it) {
        summer += *it;
      }
    }
  }

  printit(out.size());
  printit(summer);
  return 0;
}