#include <algorithm>
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

#ifdef _WIN32
#define forse_inline __forceinline inline
#else
#define forse_inline __attribute__((always_inline)) inline
#endif

bool isplain(char c) { return c != '\n' && std::isspace(c); }

class PPIterator {
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
  PPIterator(std::string_view str)
      : begin(str.begin()), end(str.end()), it(begin) {
    last = begin == end ? end : std::prev(end);
  }

  PPIterator(iterator begin, iterator end, iterator current)
      : begin(begin), end(end), it(current) {}

  reference operator*() const { return *it; }

  inline bool next_at(char next) const {
    return std::next(it) != end && *std::next(it) == next;
  }
  inline bool has_next() const { return std::next(it) < end; }
  inline bool unsafe_next_at(char next) const { return *std::next(it) == next; }

  inline bool at(char c) const { return *it == c; }
  inline bool at(char c1, char c2) const { return at(c1) && next_at(c2); }
  inline bool at(char c1, char c2, char c3) const {
    return at(c1, c2) &&  //
           std::next(it, 2) != end && *std::next(it, 2) == c3;
  }
  inline bool unsafe_at(char c1) const { return at(c1); }
  inline bool unsafe_at(char c1, char c2) const {
    return unsafe_at(c1) && unsafe_next_at(c2);
  }

  unsigned offset() const { return it - begin; }
  unsigned line() const { return nline; }

  std::string_view substr(unsigned from_offset) const {
    return {begin + from_offset, offset() - from_offset};
  }

  bool eof() const { return it == end; }
  bool islast() const { return it == last; }

  Position position() const {
    return Position{.offset = offset(),
                    .line = nline,
                    .column = offset() - line_start_offset};
  }

  void skip(unsigned n = 1) { it += 1; }

  void set_newline() {
    line_start_offset = offset();
    ++nline;
  }

  void raw_shift() {
    if (*it++ == '\n') set_newline();
  }

  void shift() {
    raw_shift();
    if (!eof() && at('\\', '\n')) {
      skip(2);
      set_newline();
    }
  }

  PPIterator& operator++() {
    shift();
    return *this;
  }
};

// Type aliases
using PPIter = PPIterator;

// Helper functions
void skip_ws(PPIter& it) {
  while (!it.eof() && std::isspace(*it)) it.shift();
}

void skip_plain_ws(PPIter& it) {
  while (!it.eof() && isplain(*it)) it.shift();
}

void skip_line(PPIter& it) {
  while (!it.eof() && !it.at('\n')) it.shift();
}

void skip_line_comment(PPIter& it) {
  totaltimeit;
  timeit;
  skip_line(it);
}

void skip_multiline_comment(PPIter& it) {
  it.skip(2);
  while (!it.islast() && !it.at('*', '/')) it.raw_shift();
  if (it.eof()) return;
  it.skip();
  it.shift();
}

void skip_string_like_literal(PPIter& it, bool ppline = false) {
  const char quot = *it;
  while (it.shift(), !it.eof()) {
    if (it.at(quot)) {
      it.shift();
      return;
    }
    if (it.at('\\')) {
      if (it.shift(), it.eof()) return;
      continue;
    }
    if (ppline && it.at('\n')) return;
  }
}

void skip_ppline_extras(PPIter& it) {
  while (!it.eof()) {
    if (it.at('/', '*')) {
      skip_multiline_comment(it);
      continue;
    }
    if (!isplain(*it)) return;
    it.shift();
  }
}
void skip_ppline_extras2(PPIter& it) {
  while (!it.eof()) {
    if (it.at('/', '*')) {
      skip_multiline_comment(it);
      continue;
    }
    if (!isplain(*it)) return;
    it.shift();
  }
}

void skip_ppline(PPIter& it) {
  while (!it.eof() && !it.at('\n')) {
    if (it.at('/', '*')) {
      skip_multiline_comment(it);
      continue;
    }
    if (it.at('/', '/')) {
      skip_line_comment(it);
      continue;
    }
    if (it.at('"') || it.at('\'')) {
      skip_string_like_literal(it, true);
      continue;
    }
    it.shift();
  }
}

std::string_view get_pp_line(PPIter& it) {
  const unsigned start = it.offset();
  skip_ppline(it);
  return it.substr(start);
}

void skip_identifier(PPIter& it) {
  if (!std::isalpha(*it) && *it != '_') return;
  while (++it, !it.eof() && (std::isalnum(*it) || *it == '_'))
    ;
}

std::string_view get_identifier(PPIter& it) {
  unsigned start = it.offset();
  skip_identifier(it);
  return it.substr(start);
}

// INCLUDE PROCESSING
bool consume_include_string(PPIter& it) {
  const char end = it.at('<') ? '>' : (it.at('"') ? '"' : 0);
  if (!end) return false;
  while (++it, !it.eof() && !it.at('\n')) {
    if (it.at(end)) {
      it.shift();
      return true;
    }
  }
  return false;
}

std::string_view get_include_string(PPIter& it) {
  unsigned start = it.offset();
  if (!consume_include_string(it)) return {};
  return it.substr(start);
}

bool process_include(PPIter& it) {
  skip_ppline_extras(it);
  auto include_string = get_include_string(it);
  skip_ppline(it);
  return !include_string.empty();
}

// DEFINE PROCESSING
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

bool get_macro_args(PPIter& it, MacroView& macro) {
  macro.is_functional = it.at('(');
  if (!macro.is_functional) return true;

  while (++it, !it.eof()) {
    skip_ppline_extras(it);
    if (it.at(')')) break;

    macro.args.push_back(get_identifier(it));
    skip_ppline_extras(it);

    if (it.at('.')) {
      macro.has_va_arg = true;
      for (int n = 2; !it.eof() && n; --n) {
        it.shift();
        if (!it.at('.')) return false;
      }
      skip_ppline_extras(it);
      break;
    }

    if (macro.args.back().empty()) return false;
    if (!it.at(',')) break;
  }

  if (!it.at(')')) return false;
  it.shift();
  return true;
}

bool process_define(PPIter& it) {
  do {
    skip_ppline_extras(it);
    MacroView macroView;
    macroView.name = get_identifier(it);
    if (macroView.name.empty()) break;
    if (!get_macro_args(it, macroView)) break;
    macroView.expansion = get_pp_line(it);
    return true;
  } while (false);
  skip_ppline(it);
  return false;
}

bool process_directive(PPIter& it) {
  if (it.eof()) return false;
  it.shift();
  skip_ppline_extras(it);
  std::string_view directive = get_identifier(it);

  if (directive == "define") return process_define(it);
  if (directive == "include") return process_include(it);
  skip_ppline(it);
  return false;
}

bool process_code(PPIter& it, std::string& out) {
  out.clear();

  if (it.eof()) return true;

  bool line_start = true;
  unsigned last_dump_offset = 0;
  auto dump = [&out, &it, &last_dump_offset]() {
    out.append(it.substr(last_dump_offset));
    last_dump_offset = it.offset();
  };

  while (!it.islast()) {
    if (std::isspace(*it)) {
      if (*it == '\n') line_start = true;
      it.shift();
      continue;
    }

    if (line_start && *it == '#') {
      dump();
      const unsigned line = it.line();
      process_directive(it);
      out.append(it.line() - line, '\n');
      last_dump_offset = it.offset();
      continue;
    }

    if (it.unsafe_at('/', '/')) {
      dump();
      const unsigned line = it.line();
      skip_line_comment(it);
      out.append(it.line() - line, '\n');
      last_dump_offset = it.offset();
      continue;
    }

    if (it.unsafe_at('/', '*')) {
      dump();
      const Position pos = it.position();
      skip_multiline_comment(it);
      out.append(it.line() - pos.line, '\n');
      out.append(it.offset() - std::max(it.offset() - pos.column,
      pos.offset),
                 ' ');
      last_dump_offset = it.offset();
      continue;
    }

    if (*it == '\'' || *it == '"') {
      line_start = false;
      skip_string_like_literal(it);
      // it.shift();
      continue;
    }

    if (std::isalpha(*it) || *it == '_') {
      line_start = false;
      auto identifier = get_identifier(it);
      dump();
      continue;
    }

    line_start = false;
    it.shift();
  }

  return true;
}

int main(int argc, char* argv[]) {
  timeit;
  checkin;
  //~8.9 Mb
  std::string src = read_file(ROOT "/sqliteall.c");
  printit(src.size());
  std::string out;

  if (true) {
    timeit;
    PPIter it{src};
    process_code(it, out);
  }
  write_file(ROOT "/sqliteall.pp.c", out);

  size_t summer = 0;
  if (true) {
    //~8.9 Gb benchmark
    timeit;
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
      for (PPIter it(src); !it.eof(); it.raw_shift()) {
        summer += *it;
      }
    }
  }

  if (true) {
    //~8.9 Gb benchmark
    stimeit("for: shift");
    std::string out;
    for (int i = 0; i < 100; ++i) {
      for (PPIter it(src); !it.eof(); it.shift()) {
        summer += *it;
      }
    }
  }

  printit(out.size());
  printit(summer);
  return 0;
}