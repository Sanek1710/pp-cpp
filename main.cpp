#include <algorithm>
#include <cstddef>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "Cursor.h"
#include "TokenPrinter.h"
#include "helper.h"

// #define debug

struct fasthash {
  size_t operator()(const std::string_view& sv) const {
    // return std::hash<std::string_view>{}(sv);
    return sv.size() < 8
               ? std::hash<std::string_view>{}(sv)
               : ((*reinterpret_cast<const uint64_t*>(sv.data()) << 4) |
                  *reinterpret_cast<const uint64_t*>(sv.end() - 8) | sv.size());
    //  : ((*reinterpret_cast<const uint64_t*>(sv.begin()) << 4) |
    //     *reinterpret_cast<const uint64_t*>(sv.rend().base() - 8));
  }
};

ankerl::unordered_dense::set<std::string_view, fasthash> defnames;

void process_code(std::string_view src) {
  Tokeniser tokeniser{src};
  // static bool halt = false;
  // if (halt) return;
  // halt = true;
  TokenPrinter printer{std::cerr, true};

  std::vector<Tokeniser::DefineImage> defines;
  std::vector<Tokeniser::IncludeImage> includes;
  std::vector<Tokeniser::UndefImage> undefs;

  bool read = true;
  Token token;
  std::vector<Token> tokens;

  while (true) {
    // if (halt) break;
    // skip_extras<false>();
    token = read ? tokeniser.read_token() : token;
    read = true;
    // printer.print(token, src);

    if (token.id == Token::eof) break;
    if (token.id == Token::pp_include) {
      includes.push_back(tokeniser.includeImage);
      continue;
      std::cerr << "#include " << tokeniser.includeImage.include_str;
      std::cerr << "\n\n";
      continue;
    }
    if (token.id == Token::pp_define) {
      defines.push_back(tokeniser.defineImage);
      defnames.emplace(tokeniser.defineImage.name);
      // std::cout << tokeniser.defineImage.name << "\n";
      continue;
      std::cerr << "#define " << tokeniser.defineImage.name;
      std::cerr << "(";
      if (!tokeniser.defineImage.args.empty()) {
        auto argit = tokeniser.defineImage.args.begin();
        std::cerr << *argit;
        for (++argit; argit != tokeniser.defineImage.args.end(); ++argit) {
          std::cerr << ", " << *argit;
        }
      }
      std::cerr << ") ";
      for (auto exp : tokeniser.defineImage.expansion) {
        std::cerr << exp;
      }
      std::cerr << "\n\n";
      continue;
    }
    if (token.id == Token::pp_undef) {
      undefs.push_back(tokeniser.undefImage);
      defnames.erase(tokeniser.defineImage.name);
      continue;
      std::cerr << "#undef " << tokeniser.undefImage.name;
      std::cerr << "\n\n";
      continue;
    }

    if (token.id == Token::identifier) {
      auto identifier_text = token.get_text(src);
      // if (!defnames.contains(identifier_text)) continue;
      tokens.clear();
      tokens.push_back(token);

      while (tokens.back().id != Token::eof) {
        auto& token = tokens.emplace_back(tokeniser.read_token());
        if (!is_extra(token.id)) break;
      }
      if (tokens.back().id != '(') {
        token = tokens.back();
        read = false;
        continue;
      }

      unsigned balance = 1;
      while (tokens.back().id != Token::eof) {
        auto& token = tokens.emplace_back(tokeniser.read_token());
        if (token.id == '(')
          ++balance;
        else if (token.id == ')')
          --balance;
        else if (token.id == ',' && balance == 1) {
          
        }
        if (!balance) break;
      }
      // printer.getos() << "call: ";
      // for (auto& token : tokens) {
      //   printer.print(token, src);
      // }
      // printer.getos() << "\n";
      continue;
    }
  }
  static bool printed = false;
  if (!printed) {
    std::cerr << "includes: " << includes.size() << "\n";
    std::cerr << "defines : " << defines.size() << "\n";
    std::cerr << "undefs  : " << undefs.size() << "\n";
    printed = true;
  }
}

int main(int argc, char* argv[]) {
  timeit;
  checkin;
  defnames.emplace("if");
  defnames.emplace("for");
  defnames.emplace("while");
  defnames.emplace("switch");
  defnames.emplace("constexpr");
//~8.9 Mb
#ifdef debug
  std::string src = read_file(ROOT "/Cursor.h");
  src += read_file(ROOT "/helper.h");
#else
  std::string src = read_file(ROOT "/sqliteall.c");
#endif
  printit(src.size());
  std::string out;

  if (true) {
    timeit;
    process_code(src);
    // printit(it.nleft());
  }
  write_file(ROOT "/out.pp.c", out);
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