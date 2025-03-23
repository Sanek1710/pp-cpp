#include <unistd.h>

#include <algorithm>
#include <bitset>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "Cursor.h"
#include "Macro.h"
#include "PositionMap.h"
#include "Token.h"
#include "TokenGroup.h"
#include "TokenPrinter.h"
#include "helper.h"
#include "util.h"


// Global maps for macro processing
StringSet defnames;
StringMap<std::string> macromap;

// Code output handler
struct CodeDumper {
  CodeDumper(size_t reserve_size = 0) { out.reserve(reserve_size); }

  void align_dump(const Token& token, std::string_view src) {
    const auto& pos = token.range.start_pos;
    if (!align_to(pos)) posmap[last_pos] = pos;

    out += token.get_text(src);
    const auto dline = token.range.end_pos.line - token.range.start_pos.line;
    last_pos.line += dline;
    if (dline == 0) {
      last_pos.column +=
          token.range.end_pos.column - token.range.start_pos.column;
    } else {
      last_pos.column = token.range.end_pos.column;
    }
  }

  void align_dump(std::string_view code, Position pos) {
    if (!align_to(pos)) posmap[last_pos] = pos;
    insert(code);
  }

  Position last_pos;
  std::string out;
  PositionMap posmap;

 private:
  bool align_to(Position pos) {
    if (last_pos.line == pos.line) {
      if (last_pos.column == pos.column) return true;
      if (last_pos.column > pos.column) return false;
      out.append(pos.column - last_pos.column, ' ');
      last_pos.column = pos.column;
      return true;
    }
    if (last_pos.line < pos.line) {
      out.append(pos.line - last_pos.line, '\n');
      out.append(pos.column, ' ');
      last_pos = pos;
      return true;
    }
    return false;
  }

  void insert(std::string_view code) {
    for (char c : code) {
      ++last_pos.column;
      if (c == '\n') {
        ++last_pos.line;
        last_pos.column = 0;
      }
      out += c;
    }
  }
};

// Token retrieval queue for macro processing
struct RetrievalQueue {
  RetrievalQueue(Tokeniser& tokeniser) : tokeniser(tokeniser) {}

  Token read() {
    if (!outers.empty()) return pop_outers();
    if (inners.empty()) return tokeniser.read_token();
    outers.assign(inners.rbegin(), inners.rend());
    inners.clear();
    return pop_outers();
  }

  Token& shadow_read() { return inners.emplace_back(tokeniser.read_token()); }

  size_t shadow_offset() const { return inners.size(); }
  void shadow_push(const Token& token) { inners.push_back(token); }

 private:
  std::vector<Token> inners;
  std::vector<Token> outers;
  Tokeniser& tokeniser;

  Token pop_outers() {
    Token token = outers.back();
    outers.pop_back();
    return token;
  }
};

// Main preprocessing function
void process_code_full(std::string_view src) {
  CodeDumper codeDumper{src.size()};
  Tokeniser tokeniser{src};
  RetrievalQueue rq(tokeniser);

  std::vector<std::string> includes;
  std::vector<size_t> arg_start_ids;
  Token token;

  while (true) {
    token = rq.read();

    if (token.id == tag::eof) break;

    if (token.id == tag::pp_include) {
      includes.emplace_back(tokeniser.includeImage.name.get_text(src));
      continue;
    }

    if (token.id == tag::pp_define) {
      macromap.emplace(tokeniser.defineImage.name.get_text(src),
                       compile_macro_expansion(tokeniser.defineImage, src));
      continue;
    }

    if (token.id == tag::pp_undef) {
      macromap.erase(tokeniser.defineImage.name.get_text(src));
      continue;
    }

    if (token.id == tag::identifier) {
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

      // Handle functional macro
      Tag ltok_id = rq.shadow_read().id;
      while (tag::is_extra(ltok_id)) {
        ltok_id = rq.shadow_read().id;
      }

      if (ltok_id != tag::raw('(')) {
        codeDumper.align_dump(token, src);
        continue;
      }

      arg_start_ids.clear();
      unsigned balance = 1;
      while (ltok_id != tag::eof) {
        ltok_id = rq.shadow_read().id;
        if (ltok_id == tag::raw('('))
          ++balance;
        else if (ltok_id == tag::raw(')'))
          --balance;
        else if (ltok_id == tag::raw(',') && balance == 1) {
          // Handle argument separation
        }
        if (!balance) break;
      }

      codeDumper.align_dump(token, src);
      continue;
    }

    codeDumper.align_dump(token, src);
  }

  // Write output to stdout
  // std::cout << codeDumper.out;
}

size_t account = 0;

template <typename T>
struct vector1 {
  T& emplace_back(const T& t) { return val = t; }
  T& back() { return val; }
  void clear() {}
  size_t size() const { return 1; }
  T val;
};

vector1<Token> tokens;
// std::vector<Token> tokens;
StringSet macronames;
StringMap<std::string> macroMap;

void process_code(std::string_view src) {
  tokens.clear();
  size_t h = 0;
  Tokeniser tkz{src};
  while (tokens.emplace_back(tkz.read_token()).id != tag::eof) {
    switch (tokens.back().id) {
      case tag::pp_include: {
        // totaltimeit;
        continue;
      }
      case tag::pp_define: {
        // totaltimeit;
        const auto macro_name = tkz.defineImage.name.get_text(src);
        // macronames.insert(macro_name);
        macroMap.emplace(macro_name,
                         compile_macro_expansion(tkz.defineImage, src));
        continue;
      }
      case tag::pp_undef: {
        // totaltimeit;
        macroMap.erase(tkz.undefImage.name.get_text(src));
        continue;
      }
      case tag::identifier: {
        const auto macro_name = tokens.back().get_text(src);
        auto macroIt = macroMap.find(macro_name);
        if (macroIt == macroMap.end()) break;  // from switch
        continue;
      }
      default:
        break;  // from switch
    }
  };

  int nidentifiers = 0;
  // for (const auto& tok : tokens) {
  //   if (tok.id == tag::identifier) {
  //     const auto macro_name = tok.get_text(src);
  //     auto macroIt = macronames.find(macro_name);
  //     // if (macroIt == macronames.end()) continue;
  //     ++nidentifiers;
  //   }
  // }

  once {
    // printit(macronames.size());
    account += tokens.size();
    account += nidentifiers;
    printit(nidentifiers);
    // std::cerr << std::hex << h;
    // printit(h);
  };
}

int perf_test() {
  std::string src = read_file(ROOT "pp.test/sqliteall.c");
  timeit;
  repeat(5) {
    repeat(20) { process_code(src); }
    untimeit;
    usleep(500'000);
  }
  printit(account);
  return 0;
}

int user_test() {
  std::string src = read_file(ROOT "pp.test/pp.in.cpp");
  timeit;
  repeat(100) { process_code(src); }
  return 0;
}

int main(int argc, char* argv[]) {
  perf_test();
  user_test();
}

int main_cli(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <input_file>\n";
    return 1;
  }

  // Read input file
  std::string src = read_file(argv[1]);
  if (src.empty()) {
    std::cerr << "Failed to read input file: " << argv[1] << "\n";
    return 1;
  }

  // Process the code
  process_code(src);

  return 0;
}

testit(compile_macro_expansion) {
  // return;
  std::string src = read_file("/mnt/d/Projects/pp-cpp/pp.test/pp.in.cpp");

  StringMap<std::string> expected =  //
      {
          {"TWO_WORDS", "f0 aa bb"},                 //
          {"STR_AFTER_TOKEN", "f1 aa $0s"},          //
          {"NAMED_VA", "v1 some($0 )"},              //
          {"UNNAMED_VA", "v1 some($0 )"},            //
          {"INCNAMED_VA", "v1 some(__VA_ARGS__)"},   //
          {"INCUNNAMED_VA", "v1 some(named)"},       //
          {"CNSTINBOOL", " constexpr inline bool"},  //
          {"farg", "f1 farg($0 )"},                  //
          {"f", "f0 f()"},                           //
          {"abeta", "f1 alpha$0_gamma"},             //
          {"SELF1", "f2 ($0 , $1 )"},                //
          {"CAT1", "f2 $0_$1_"},                     //
          {"STR1", "f1 $0s"},                        //
          {"STR1", "f1 $0s"},                        //
          {"CATSTR1", "f2 $0 $1s"},                  //
          {"CATSTR2", "f2 $0_$1s"},                  //
          {"CATSTR3", "f2 $0_$1s"},                  //
          {"CATSTR4", "f2 $0s$1s"},                  //
          {"CATSTR5", "f2 $0_$1_"},                  //
      };

  Tokeniser tkz{src};
  Token token = tkz.read_token();
  size_t nfailed = 0;
  while (token.id != tag::eof) {
    if (token.id == tag::pp_define) {
      std::string_view name = tkz.defineImage.name.get_text(src);
      std::string act = compile_macro_expansion(tkz.defineImage, src);
      tkz.defineImage.print(std::cerr, src);
      std::cerr << "\n";

      if (expected.contains(name)) {
        auto exp = expected.at(name);
        if (act == exp) {
          std::cerr << "\033[32m[pass]\033[0m  act: `" << act << "`\n";
          std::cerr << "\033[32m      \033[0m  exp: `" << exp << "`\n";
        } else {
          ++nfailed;
          std::cerr << "\033[31m[fail]\033[0m  act: `" << act << "`\n";
          std::cerr << "\033[31m      \033[0m  exp: `" << exp << "`\n";
        }
      } else {
        std::cerr << "  act: `" << act << "`\n";
      }
      std::cerr << "\n";
    }
    token = tkz.read_token();
  }
  if (nfailed) {
    std::cerr << "\033[31m[failed:]\033[0m: " << nfailed << "\n";
  }

  // exit(0);
}

// testit(compile_macro_expansion_v2) {
//   std::string src = R"(
//     #define CAT(x, y)  x##y
//     #define STR(x)  #x
//     #define CATSTR(x, y) #x ## # y
//   )";

//   Tokeniser tkz{src};
//   Token token = tkz.read_token();
//   while (token.id != tag::eof) {
//     if (token.id == tag::pp_define) {
//       std::cerr << std::setw(10) << tkz.defineImage.name.get_text(src) << ":
//       "; std::cerr << compile_macro_expansion_v2(tkz.defineImage, src) <<
//       "\n";
//     }
//     token = tkz.read_token();
//   }
// }