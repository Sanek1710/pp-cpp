#include <algorithm>
#include <bitset>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "Cursor.h"
#include "Macro.h"
#include "PositionMap.h"
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

    if (token.id == Token::eof) break;

    if (token.id == Token::pp_include) {
      includes.emplace_back(tokeniser.includeImage.name.get_text(src));
      continue;
    }

    if (token.id == Token::pp_define) {
      macromap.emplace(tokeniser.defineImage.name.get_text(src),
                       compile_macro_expansion(tokeniser.defineImage, src));
      continue;
    }

    if (token.id == Token::pp_undef) {
      macromap.erase(tokeniser.defineImage.name.get_text(src));
      continue;
    }

    if (token.id == Token::identifier) {
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
      token_id ltok_id = rq.shadow_read().id;
      while (is_extra(ltok_id)) {
        ltok_id = rq.shadow_read().id;
      }

      if (ltok_id != '(') {
        codeDumper.align_dump(token, src);
        continue;
      }

      arg_start_ids.clear();
      unsigned balance = 1;
      while (ltok_id != Token::eof) {
        ltok_id = rq.shadow_read().id;
        if (ltok_id == '(')
          ++balance;
        else if (ltok_id == ')')
          --balance;
        else if (ltok_id == ',' && balance == 1) {
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

// vector1<Token> tokens;
std::vector<Token> tokens;
StringSet macronames;

void process_code(std::string_view src) {
  tokens.clear();
  size_t h = 0;
  Tokeniser tkz{src};
  while (tokens.emplace_back(tkz.read_token()).id != Token::eof) {
    switch (tokens.back().id) {
      case Token::pp_include:
        continue;
      case Token::pp_define: {
        const auto macro_name = tkz.defineImage.name.get_text(src);
        macronames.insert(macro_name);
        continue;
      }
      case Token::pp_undef: {
        macronames.erase(tkz.undefImage.name.get_text(src));
        continue;
      }
      case Token::identifier: {
        const auto macro_name = tokens.back().get_text(src);
        auto macroIt = macronames.find(macro_name);
        if (macroIt == macronames.end()) break;  // from switch

        continue;
      }
      default:
        break;  // from switch
    }
  };

  int nidentifiers = 0;
  // for (const auto& tok : tokens) {
  //   if (tok.id == Token::identifier) {
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
  repeat(100) { process_code(src); }
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