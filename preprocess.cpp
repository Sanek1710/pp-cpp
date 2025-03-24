#include <unistd.h>

#include <algorithm>
#include <bitset>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
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

    if (token.tag == tag::eof) break;

    if (token.tag == tag::pp_include) {
      includes.emplace_back(tokeniser.includeImage.name.get_text(src));
      continue;
    }

    if (token.tag == tag::pp_define) {
      macromap.emplace(tokeniser.defineImage.name.get_text(src),
                       compile_macro_expansion(tokeniser.defineImage, src));
      continue;
    }

    if (token.tag == tag::pp_undef) {
      macromap.erase(tokeniser.defineImage.name.get_text(src));
      continue;
    }

    if (token.tag == tag::identifier) {
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
      Tag ltok_id = rq.shadow_read().tag;
      while (tag::is_extra(ltok_id)) {
        ltok_id = rq.shadow_read().tag;
      }

      if (ltok_id != tag::raw('(')) {
        codeDumper.align_dump(token, src);
        continue;
      }

      arg_start_ids.clear();
      unsigned balance = 1;
      while (ltok_id != tag::eof) {
        ltok_id = rq.shadow_read().tag;
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

// std::vector<Token> tokens;
StringSet macronames;
StringMap<std::string> macroMap;
std::vector<Token> tokens{1};

// for the time being i dont wanna do afterscan
// but once one decides to do it:
// - replace vector with deque
// - invoke afterscan as funcion finalise
// - expand token groups on place in deque
// that is likely the best way
// maybe not
Token expand_macro(Tokeniser& tkz, MacroStamp macroStamp,
                   std::deque<Token>& out) {
  std::string_view src = tkz.get_src();

  Token token = tkz.read_token();
  if (!macroStamp.info.is_functional) {
    // just add expansion and return
    // for now i just return next token as that is what expected
    return token;
  }
  // here we skipping all extras, without writing them anywhere
  // if one need them for some reason, one should implement smarter dump logic
  // good luck
  while (tag::is_extra(token.tag)) token = tkz.read_token();
  if (token.tag != tag::raw('(')) return token;

  std::vector<size_t> arg_rages;
  arg_rages.push_back(out.size());

  int balance = 0;
  bool successfullness = false;
  while (token.tag != tag::eof) {
    if (tag::is_extra(token.tag)) {
      token = tkz.read_token();
      continue;
    }
    switch (token.tag) {
      case tag::raw('('): {
        ++balance;
        break;
      }
      case tag::raw(')'): {
        --balance;
        if (!balance) {
          // remember end position for arg
          arg_rages.push_back(out.size());
          // apply macro
          // or dump saved tokens if cant expand
          // or (3rd thing i forgot)
          // TODONOW: write when remember
          out.push_back(token);
          // setup for loop exit:
          successfullness = true;
          token.tag = tag::eof;
          continue;  // in fact break from while loop
        }
        break;
      }
      case tag::raw(','): {
        if (balance == 1) arg_rages.push_back(out.size());
        break;
      }

      case tag::identifier: {
        const auto macro_name = token.get_text(src);
        auto macroIt = macroMap.find(macro_name);
        if (macroIt == macroMap.end()) break;

        out.push_back(token);
        // token = expand_macro(tkz, MacroStamp{macroIt->second}, out);
        token = tkz.read_token();

        // out[0].print(tkz.get_src(), std::cerr);
        // token.print(tkz.get_src(), std::cerr);
        // exit(0);
        continue;
      }
    }
    out.push_back(token);
    token = tkz.read_token();
  }

  // cant expand
  const size_t nargs_input = arg_rages.size() - 1;
  if (!successfullness || macroStamp.is_valid_call(nargs_input)) {
    // maybe find better match if such exists?
    // dump all the deque
    // but actually seems like just leave it as is and return next token
    return tkz.read_token();
  }

  // to this point we have whole bunch of expansion relevant tokens
  // also marked argument ranges

  // also argument count

  // what do we do with it?

  return tkz.read_token();
}

void process_code(std::string_view src) {
  size_t h = 0;
  Tokeniser tkz{src};
  tokens.resize(1);

#define dump(token, tokens)                            \
  if (tokens.back().range.end == token.range.start) {  \
    tokens.back().range.end = token.range.end;         \
    tokens.back().range.end_pos = token.range.end_pos; \
  } else {                                             \
    tokens.push_back(token);                           \
  }
  Token token = tkz.read_token();
  while (token.tag != tag::eof) {
    switch (token.tag) {
      case tag::pp_include: {
        // totaltimeit;
        break;
      }
      case tag::pp_define: {
        // totaltimeit;
        const auto macro_name = tkz.defineImage.name.get_text(src);
        // macronames.insert(macro_name);
        macroMap.emplace(macro_name,
                         compile_macro_expansion(tkz.defineImage, src));
        break;
      }
      case tag::pp_undef: {
        // totaltimeit;
        macroMap.erase(tkz.undefImage.name.get_text(src));
        break;
      }
      case tag::identifier: {
        const auto macro_name = token.get_text(src);
        auto macroIt = macroMap.find(macro_name);
        if (macroIt == macroMap.end()) break;  // from switch

        std::deque<Token> expansion{1, token};
        token = expand_macro(tkz, MacroStamp{macroIt->second}, expansion);
        // expand macro
        for (auto tok : expansion) {
          tok.print(src, std::cerr);
        }
        std::cerr << "\n";
        dump(token, tokens);
        continue;
      }
      default:
        // dumb dump
        dump(token, tokens);
        break;  // from switch
    }
    token = tkz.read_token();
  };

  once {
    // printit(macronames.size());
    account += tokens.size();
    printit(tokens.size());
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
  process_code(src);
  return 0;
}

int main(int argc, char* argv[]) {
  // perf_test();
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
  while (token.tag != tag::eof) {
    if (token.tag == tag::pp_define) {
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