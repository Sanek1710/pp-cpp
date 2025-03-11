#include <cstddef>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "Cursor.h"
#include "TokenPrinter.h"
#include "helper.h"

// #define debug

void process_code(std::string_view src) {
  Tokeniser tokeniser{src};
  // static bool halt = false;

  // TokenPrinter printer{std::cerr};

  std::vector<Tokeniser::DefineImage> defines;
  std::vector<Tokeniser::IncludeImage> includes;
  std::vector<Tokeniser::UndefImage> undefs;

  while (true) {
    // if (halt) break;
    // skip_extras<false>();
    Token token = tokeniser.read_token();

    if (token.id == token::eof) break;
    if (token.id == token::pp_include) {
      includes.push_back(tokeniser.includeImage);
      continue;
      std::cerr << "#include " << tokeniser.includeImage.include_str;
      std::cerr << "\n\n";
      continue;
    }
    if (token.id == token::pp_define) {
      defines.push_back(tokeniser.defineImage);
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
    if (token.id == token::pp_undef) {
      undefs.push_back(tokeniser.undefImage);
      continue;
      std::cerr << "#undef " << tokeniser.undefImage.name;
      std::cerr << "\n\n";
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
    repeat(10) {
      repeat(10) { process_code(src); }
      // untimeit;
      // usleep(200000);
    }
    summer += out.size();
  }

  printit(out.size());
  printit(summer);
  return 0;
}