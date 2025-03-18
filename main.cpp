#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "Cursor.h"
#include "PositionMap.h"
#include "TokenPrinter.h"
#include "helper.h"
#include "util.h"
#include "Macro.h"
#include "CodeDumper.h"
// #define debug

StringMap<std::string> macromap;

struct Macro {
  std::string name;
  std::string expansion;
};

// struct CodeDumper {
//  public:
//   CodeDumper(size_t reserve_size = 0) { out.reserve(reserve_size); }

//   inline void dump(const Token& token, std::string_view src) {
//     const auto& pos = token.range.start_pos;
//     if (pos != last_pos) posmap[last_pos] = pos;

//     out += token.get_text(src);
//     const auto dline = token.range.end_pos.line - token.range.start_pos.line;
//     last_pos.line += dline;
//     if (dline == 0) {
//       last_pos.column +=
//           token.range.end_pos.column - token.range.start_pos.column;
//     } else {
//       last_pos.column = token.range.end_pos.column;
//     }
//   }

//   inline void align_dump(const Token& token, std::string_view src) {
//     const auto& pos = token.range.start_pos;
//     if (!align_to(pos)) posmap[last_pos] = pos;

//     out += token.get_text(src);
//     const auto dline = token.range.end_pos.line - token.range.start_pos.line;
//     last_pos.line += dline;
//     if (dline == 0) {
//       last_pos.column +=
//           token.range.end_pos.column - token.range.start_pos.column;
//     } else {
//       last_pos.column = token.range.end_pos.column;
//     }
//   }

//   inline void dump(std::string_view code, Position pos) {
//     if (pos != last_pos) posmap[last_pos] = pos;
//     insert(code);
//   }

//   void align_dump(std::string_view code, Position pos) {
//     if (!align_to(pos)) posmap[last_pos] = pos;
//     insert(code);
//   }

//   //  private:
//   Position last_pos;
//   std::string out;
//   PositionMap posmap;

//   inline bool align_to(Position pos) {
//     if (last_pos.line == pos.line) {
//       if (last_pos.column == pos.column) return true;
//       if (last_pos.column > pos.column) return false;
//       out.append(pos.column - last_pos.column, ' ');
//       last_pos.column = pos.column;
//       return true;
//     }
//     if (last_pos.line < pos.line) {
//       out.append(pos.line - last_pos.line, '\n');
//       out.append(pos.column, ' ');
//       last_pos = pos;
//       return true;
//     }
//     return false;
//   }

//   inline void insert(std::string_view code) {
//     for (char c : code) {
//       ++last_pos.column;
//       if (c == '\n') {
//         ++last_pos.line;
//         last_pos.column = 0;
//       }
//       out += c;
//     }
//   }
//   inline void insert(size_t n, char c) {
//     if (c == '\n') {
//       last_pos.line += n;
//       last_pos.column = 0;
//     } else {
//       last_pos.column += n;
//     }
//     out.append(n, c);
//   }
// };



struct RetrievalQueue {
  RetrievalQueue(Tokeniser& tokeniser) : tokeniser(tokeniser) {}

  Token read() {
    if (!outers.empty()) return pop_outers();
    if (inners.empty()) return tokeniser.read_token();
    outers.assign(inners.rbegin(), inners.rend());
    inners.clear();
    return pop_outers();
  }

  Token& shadow_read() {  //
    return inners.emplace_back(tokeniser.read_token());
  }
  size_t shadow_offset() const { return inners.size(); }

  void shadow_push(const Token& token) { inners.push_back(token); }

  std::vector<Token> inners;
  std::vector<Token> outers;
  Tokeniser& tokeniser;

  inline Token pop_outers() {
    Token token = outers.back();
    outers.pop_back();
    return token;
  }
};

void process_code(std::string_view src) {
  CodeDumper codeDumper{src.size()};

  Tokeniser tokeniser{src};
  RetrievalQueue rq(tokeniser);
  // static bool halt = false;
  // if (halt) return;
  // halt = true;
  TokenPrinter printer{std::cerr, true};

  std::vector<DefineImage> defines;
  std::vector<std::string> includes;
  std::vector<UndefImage> undefs;

  Token token;

  std::vector<size_t> arg_start_ids;
  while (true) {
    // skip_extras<false>();
    token = rq.read();
    // codeDumper.out += token.get_text(src);
    // printer.print(token, src);

    if (token.id == Token::eof) break;
    if (token.id == Token::pp_include) {
      // continue;
      includes.emplace_back(tokeniser.includeImage.name.get_text(src));
      continue;
    }
    if (token.id == Token::pp_define) {
      // continue;
      macromap.emplace(tokeniser.defineImage.name.get_text(src),
                       compile_macro_expansion(tokeniser.defineImage, src));
      continue;
    }
    if (token.id == Token::pp_undef) {
      // continue;
      macromap.erase(tokeniser.defineImage.name.get_text(src));
      continue;
    }

    if (token.id == Token::identifier) {
      // continue;
      auto identifier_text = token.get_text(src);
      auto macroIt = macromap.find(identifier_text);
      if (macroIt == macromap.end()) {
        codeDumper.align_dump(token, src);
        continue;
      }

      MacroStamp macroStamp{macroIt->second};
      if (!macroStamp.info.is_functional) {
        codeDumper.align_dump(macroStamp.expansion, token.range.start_pos);
        continue;
      }

      // shadow: IDENTIFIER
      token_id ltok_id = rq.shadow_read().id;
      while (is_extra(ltok_id)) {
        ltok_id = rq.shadow_read().id;
      }

      // invalid macro call, just dump identifier
      if (ltok_id != '(') {
        codeDumper.align_dump(token, src);
        continue;
      }
      // at '('
      arg_start_ids.clear();
      unsigned balance = 1;
      while (ltok_id != Token::eof) {
        ltok_id = rq.shadow_read().id;
        if (ltok_id == '(')
          ++balance;
        else if (ltok_id == ')')
          --balance;
        else if (ltok_id == ',' && balance == 1) {
        }
        if (!balance) break;
      }

      codeDumper.align_dump(token, src);
      // printer.getos() << "call: ";
      // for (auto& token : tokens) {
      //   printer.print(token, src);
      // }
      // printer.getos() << "\n";
      continue;
    }

    codeDumper.align_dump(token, src);
  }
  never {
    for (const auto& [name, exp] : macromap) {
      std::cerr << MacroStamp{exp}.info.is_functional << "\n";
      std::cerr << MacroStamp{exp}.info.is_variadic << "\n";
      std::cerr << MacroStamp{exp}.info.nargs << "\n";
      std::cerr << name << ": " << exp << "\n\n";
    }
    std::cerr << "macromap: " << macromap.size() << "\n";
    std::cerr << "includes: " << includes.size() << "\n";
    std::cerr << "defines : " << defines.size() << "\n";
    std::cerr << "undefs  : " << undefs.size() << "\n";
    std::cout << "\e[30m" << codeDumper.out << "\e[0m";
  }
}

int main(int argc, char* argv[]) {
  timeit;
  checkin;
//~8.9 Mb
#ifdef debug
  std::string src = read_file(ROOT "pp.test/pp.in.cpp");
#else
  std::string src = read_file(ROOT "pp.test/sqliteall.c");
#endif
  printit(src.size());
  std::string out;

  if (true) {
    timeit;
    process_code(src);
    // printit(it.nleft());
  }
  write_file(ROOT "pp.test/out.pp.c", out);
#ifdef debug
  return true;
#endif
  size_t summer = 0;

  if (true) {
    //~890 Mb benchmark
    stimeit("process_code 100 times");
    std::string out;
    int loading = 0;
    repeat(10) {
      repeat(10) {
        // std::cerr << ++loading << "\r";
        process_code(src);
      }
      // untimeit;
      // usleep(200000);
    }
    summer += out.size();
  }

  printit(out.size());
  printit(summer);
  return 0;
}