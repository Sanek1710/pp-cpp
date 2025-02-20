
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

template <bool skip_line_continuation = false>
struct PPCursorBase {
  using iterator = std::string_view::iterator;

  iterator begin;
  iterator end;
  iterator it;
  unsigned line_start_offset = 0;
  unsigned nline = 0;
  char c = 0;

  PPCursorBase(std::string_view str)
      : begin(str.begin()), end(str.end()), it(begin) {
    c = !str.empty() ? str.front() : 0;
  }

  constexpr char current() const { return c; }
  constexpr char next() { return std::next(it) < end ? *std::next(it) : 0; }

  constexpr bool next_at(char next) const {
    return std::next(it) < end && *std::next(it) == next;
  }

  constexpr bool at(char current) const { return c == current; }
  constexpr bool at(char current, char next) const {
    return at(current) && next_at(next);
  }
  constexpr bool at(char current, char next, char third) const {
    return at(current, next) &&  //
           std::next(it, 2) < end && *std::next(it, 2) == third;
  }

  constexpr bool atalpha() const { return std::isalpha(c); }
  constexpr bool atalnum() const { return std::isalnum(c); }
  constexpr bool atspace() const { return std::isspace(c); }
  constexpr bool atplain() const { return !at('\n') && atspace(); }
  constexpr bool atdigit() const { return std::isdigit(c); }

  constexpr unsigned offset() const { return it - begin; }
  constexpr unsigned line() const { return nline; }

  constexpr std::string_view substr(unsigned from_offset) const {
    return {begin + from_offset, offset() - from_offset};
  }

  constexpr bool eof() const { return it == end; }

  Position position() const {
    return Position{.offset = offset(),
                    .line = nline,
                    .column = offset() - line_start_offset};
  }

  // just skip, doesnt validate or process
  inline constexpr void skip(unsigned n = 1) { it += n; }

  inline bool shift() {
    if (!c) return false;
    if (at('\n')) {
      line_start_offset = offset() + 1;
      ++nline;
    }
    ++it;
    if constexpr (skip_line_continuation) {
      if (it < end && at('\\', '\n')) {
        skip(2);
        line_start_offset = offset();
        ++nline;
      }
    }
    return c = it < end ? *it : 0;
  }
};

using PPCursor = PPCursorBase<true>;
using PPRawCursor = PPCursorBase<false>;

void skip_ws(PPCursor& cursor) {
  while (cursor.atspace() && cursor.shift())
    ;
}

void skip_plain_ws(PPCursor& cursor) {
  while (cursor.atplain() && cursor.shift())
    ;
}

void skip_line(PPCursor& cursor) {
  while (!cursor.at('\n') && cursor.shift())
    ;
}

void skip_line_comment(PPCursor& cursor) { skip_line(cursor); }

void skip_multiline_comment(PPCursor& cursor) {
  cursor.skip();
  while (cursor.shift() && !cursor.at('*', '/'))
    ;
  cursor.skip();
  cursor.shift();
}

void skip_string_like_literal(PPCursor& cursor, bool ppline = false) {
  const char quot = cursor.current();
  while (cursor.shift() && !cursor.at(quot)) {
    if (cursor.at('\\'))
      if (!cursor.shift()) return;
    if (ppline && cursor.at('\n')) return;
  }
}

// tokens that pp ignores and treat the rest being on the same line
void skip_ppline_extras(PPCursor& cursor) {
  while (true) {
    if (cursor.at('/', '*')) {
      skip_multiline_comment(cursor);
      continue;
    }
    if (!cursor.atplain()) return;
    if (!cursor.shift()) return;
  }
}

// ppline is line until \n but skipping all ml comments
// for example macro expansion
void skip_ppline(PPCursor& cursor) {
  while (!cursor.at('\n')) {
    if (cursor.at('/', '*')) {
      skip_multiline_comment(cursor);
      continue;
    }
    if (cursor.at('/', '/')) {
      skip_line_comment(cursor);
      continue;
    }
    if (cursor.at('"') || cursor.at('\'')) {
      skip_string_like_literal(cursor, /*ppline=*/true);
      continue;
    }
    if (!cursor.shift()) return;
  }
}

void skip_identifier(PPCursor& cursor) {
  if (!(cursor.at('_') || cursor.atalpha())) return;
  while (cursor.shift() &&  //
         (cursor.at('_') || cursor.atalnum()))
    ;
}
std::string_view get_identifier(PPCursor& cursor) {
  unsigned start = cursor.offset();
  skip_identifier(cursor);
  return cursor.substr(start);
}

// INCLUDE PROCESSING
bool consume_include_string(PPCursor& cursor) {
  const char end = cursor.at('<') ? '>' :  //
                       (cursor.at('"') ? '"' : 0);
  if (!end) return false;
  while (cursor.shift() && !cursor.at('\n')) {
    if (cursor.at(end)) {
      cursor.shift();
      return true;
    }
  }
  return false;
}

std::string_view get_include_string(PPCursor& cursor) {
  unsigned start = cursor.offset();
  if (!consume_include_string(cursor)) return {};
  return cursor.substr(start);
}

bool process_include(PPCursor& cursor) {
  skip_ppline_extras(cursor);
  auto include_string = get_include_string(cursor);
  printit(include_string);
  return include_string.empty();
}

// DEFINE PROCESSING

struct MacroView {
  std::string_view name;
  std::string_view expansion;
  std::vector<std::string_view> args;
  bool present = false;
  bool has_va_arg = false;
  // kinda optional, can be checked afterwards
  bool error = false;

  void print(std::ostream& os) {
    if (present) os << "  (  )\n";
    if (!present) os << "  [no args]\n";
    os << "#define " << name << "\n";
    {
      bool first = true;
      os << "(";
      for (auto arg : args) {
        os << (first ? "" : ", ") << arg;
      }
      os << ")";
    }
    os << "  expand: `" << expansion << "`\n";
    if (has_va_arg) os << "    ...\n";
    if (error) os << "  [error]\n";
  }
};

bool get_macro_args(PPCursor& cursor, MacroView& macro) {
  macro.present = cursor.at('(');
  if (!macro.present) return true;
  while (cursor.shift()) {
    skip_ppline_extras(cursor);
    if (cursor.at(')')) break;
    macro.args.push_back(get_identifier(cursor));
    skip_ppline_extras(cursor);
    if (cursor.at('.')) {
      macro.has_va_arg = true;
      for (int n = 2; cursor.shift() && n; --n) {
        if (!cursor.at('.')) return false;
      }
      skip_ppline_extras(cursor);
      break;
    }
    if (macro.args.back().empty()) return false;
    if (!cursor.at(',')) break;
  }
  if (!cursor.at(')')) return false;
  cursor.shift();
  return true;
}

bool process_define(PPCursor& cursor) {
  skip_ppline_extras(cursor);
  MacroView macroView;
  macroView.name = get_identifier(cursor);
  if (macroView.name.empty()) return false;
  if (!get_macro_args(cursor, macroView)) return false;

  const unsigned start = cursor.offset();
  skip_ppline(cursor);
  macroView.expansion = cursor.substr(start);
  macroView.print(std::cerr);
  return true;
}

bool process_directive() {
  do {
    // process
    return true;
  } while (false);
  // error recovery

}

int main(int argc, char* argv[]) {
  timeit;
  checkin;
  //~8.9 Mb
  std::string src = read_file("/mnt/d/Projects/pp-cpp/sqliteall.c");
  std::cout << "Hello"
            << "\n";
  {
    //~8.9 Gb benchmark
    timeit;
    for (int i = 0; i < 1; ++i) {
      PPCursor cursor(src);
      while (cursor.shift())
        ;
    }
  }

  return 0;
}