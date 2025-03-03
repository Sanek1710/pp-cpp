#include <algorithm>
#include <cctype>
#include <deque>
#include <iostream>
#include <iterator>
#include <string_view>
#include <vector>

#include "ankerl/unordered_dense.h"
#include "helper.h"

struct Position {
  unsigned offset;
  unsigned line;
  unsigned column;
};

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
 public:
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
  Dumper() { set.reserve(9000); }
  void dump_include(std::string_view include) {
    // std::cerr << "#include " << include << "\n";
  }
  void dump_macro(MacroView&& macro_view) {  //
    // macro_view.print(std::cerr);
  }
  void dump_ref(std::string_view ref) {  //
    set.insert(ref);
    // std::cerr << "> " << ref << "\n";
  }
  void dump_code() {}
  ankerl::unordered_dense::set<std::string_view> set;
};

Dumper dumper;

class PPMicro {
  
 public:
  PPMicro(std::string_view src) : src(src), it(src) {
    end = it.end;
    last = it.last;
  }
  inline void process_code() { return skip_code</*in_directive=*/false>(); }

 private:
  std::string_view src;
  PPIter it;
  using iterator = std::string_view::iterator;
  iterator end;
  iterator last;

  inline void skip_line_comment() {
    for (++it; it.ltlast(); ++it) {
      if (*it == '\n') return;
      if (*it == '\\' && it.next() == '\n') ++it;
    }
    // handle last
    if (!it.ltend()) return;
    if (*it != '\n') ++it;
  }

  inline void skip_multiline_comment() {
    ++it;  // skip '/'
    for (++it; it.ltlast(); ++it) {
      if (*it == '*' && it.next() == '/') break;
    }
    if (it.ltlast()) ++it;  // skip '*'
    ++it;                   // skip '/' or last
  }

  inline void skip_identifier() {
    for (++it; it.ltend(); ++it) {
      if (!std::isalnum(*it) && *it != '_') return;
    }
  }
  inline bool consume_identifier() {
    if (!std::isalpha(*it) && *it != '_') return false;
    skip_identifier();
    return true;
  }

  inline void skip_number_like() {
    for (++it; it.ltend(); ++it) {
      // might be some complicated logic with [eEpP][+-] but meh
      // too uncommon plus next after them has to be number anyway
      if (!std::isalnum(*it) && *it != '_' && *it != '.') return;
    }
  }
  inline bool consume_number() {
    if (!std::isdigit(*it)) return false;
    skip_number_like();
    return true;
  }

  inline void skip_string_like_literal(bool ppline = false) {
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
    if (!it.ltend()) return;
    consume_identifier();
  }

  // add raw string literal

  inline std::string_view get_identifier() {
    unsigned start = it.offset();
    consume_identifier();
    return it.substr(start);
  }

  inline static bool is_string_prefix(std::string_view str) {
    if (str.size() > 3) return false;
    bool isRawString = str.back() == 'R';
    if (isRawString) str.remove_suffix(1);
    switch (str.size()) {
      case 0:
        return true;
      case 1:
        return str.front() == 'L' || str.front() == 'u' || str.front() == 'U';
      case 2:
        return str == "u8";
      default:
        return false;
    }
  }
  inline bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
  }
  template <bool in_directive, bool extras_only = false>
  inline void skip_code() {
    bool line_start = false;
    while (it.ltlast()) {
      if (std::isspace(*it)) {
        if (*it == '\n') {
          line_start = true;
          if constexpr (in_directive) return;
        }
        ++it;
        continue;
      }
      if constexpr (!extras_only) {
        if constexpr (!in_directive) {
          if (line_start && *it == '#') {
            process_directive();
            continue;
          }
        }
        if (std::isalnum(*it) || *it == '_') {
          line_start = false;
          if (std::isdigit(*it)) {
            skip_number_like();
            continue;
          }
          auto identifier = get_identifier();
          if (it.ltend() &&                   //
              (*it == '\'' || *it == '"') &&  //
              is_string_prefix(identifier)) {
            skip_string_like_literal();
            continue;
          }
          // handle identifier
          continue;
        }
        if (*it == '"' || it.next() == '\'') {
          line_start = false;
          skip_string_like_literal(true);
          continue;
        }
      }
      if (*it == '\\' && it.next() == '\n') {
        ++ ++it;
        continue;
      }
      if (*it == '/' && it.next() == '*') {
        skip_multiline_comment();
        continue;
      }
      if (*it == '/' && it.next() == '/') {
        skip_line_comment();
        continue;
      }
      line_start = false;
      if constexpr (extras_only)
        return;
      else
        ++it;
    }
    if (!it.ltend()) return;
    // TODO: fix not processing last el
    if (*it != '\n') ++it;
  }

  inline void skip_ppline() {
    return skip_code</*in_directive=*/true, /*extras_only=*/false>();
  }
  inline void skip_ppline_extras() {
    return skip_code</*in_directive=*/true, /*extras_only=*/true>();
  }

  // INCLUDE PROCESSING
  inline void skip_include_string() {
    const char quot = *it == '<' ? '>' :  //
                          (*it == '"' ? '"' : 0);
    if (!quot) return;
    for (++it; it.ltlast(); ++it) {
      if (*it == quot || *it == '\n') break;
      if (*it == '\\' && it.next() == '\n') ++it;
    }
    if (*it != '\n') ++it;
  }

  inline bool process_include() {
    skip_ppline_extras();
    const unsigned start = it.offset();
    skip_include_string();
    auto include_string = it.substr(start);
    skip_ppline();
    if (include_string.empty()) return false;
    dumper.dump_include(include_string);
    return true;
  }

  // DEFINE PROCESSING

  inline bool get_macro_args(MacroView& macro) {
    macro.is_functional = *it == '(';
    if (!macro.is_functional) return true;

    for (++it; it.ltend(); ++it) {
      skip_ppline_extras();
      if (!it.ltend()) return false;

      if (*it == ')') break;
      macro.args.push_back(get_identifier());
      if (!it.ltend()) return false;

      skip_ppline_extras();
      if (!it.ltend()) return false;

      // va args?
      if (*it == '.') {
        if (it.nleft() < 4) return false;
        if (*++it != '.') return false;
        if (*++it != '.') return false;
        macro.has_va_arg = true;
        ++it;
        skip_ppline_extras();
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

  inline bool process_define() {
    do {
      skip_ppline_extras();
      if (!it.ltend()) return false;
      MacroView macroView;
      macroView.name = get_identifier();
      if (!it.ltend()) return !macroView.name.empty();

      if (macroView.name.empty()) break;
      if (!get_macro_args(macroView)) break;

      const unsigned start = it.offset();
      skip_ppline();
      macroView.expansion = it.substr(start);
      dumper.dump_macro(std::move(macroView));
      return true;
    } while (false);
    skip_ppline();
    return false;
  }

  inline bool process_directive() {
    ++it;  // skip '#'
    if (!it.ltend()) return false;

    skip_ppline_extras();
    if (!it.ltend()) return false;

    std::string_view directive = get_identifier();
    if (!it.ltend()) return false;

    if (directive == "define") return process_define();
    if (directive == "include") return process_include();

    skip_ppline();
    return false;
  }
};

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
    PPMicro ppm{src};
    ppm.process_code();
    // printit(it.nleft());
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
      PPMicro ppm{src};
      ppm.process_code();
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