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
#include "StringToken.h"
#include "Token.h"
#include "TokenGroup.h"
#include "TokenPrinter.h"
#include "helper.h"
#include "util.h"

class MacroExpansionTokeniser : private Tokeniser {
 public:
  MacroExpansionTokeniser(std::string_view src) : Tokeniser(src) {}

  inline Token read_token() {
    Token token;
    token.range.start = cur.it - src.begin();
    token.range.start_pos = cur.to_position();
    token.tag = skip_next();
    if (token.tag == tag::arg) {
      cur.it = std::from_chars(cur.it, end, token.external_index).ptr;
      // fill =
      cur.it++;
    }
    token.range.end = cur.it - src.begin();
    token.range.end_pos = cur.to_position();
    return token;
  }

  inline Tag skip_next() {
    if (cur.it == end) return tag::eof;
    if (*cur.it == '$') { /*5*/
      ++cur.it;
      if (*cur.it == '$') return tag::raw(*cur.it++);
      return tag::arg;
    }
    return skip_common<false>();
  }

  inline bool eof() const { return Tokeniser::eof(); }
};

// Global maps for macro processing
class Preprocessor {
 public:
  // StringSet defnames;
  // StringMap<std::string> macromap;

  // std::vector<Token> tokens;
  // StringSet macronames;
  StringMap<std::string> macroMap;

  std::vector<StrToken> out;
  std::vector<StrToken> buffer;
  std::vector<size_t> arg_rages;
  std::string str_buffer;
  std::vector<std::string_view> macro_stack;
  // for the time being i actually dont wanna do afterscan
  // but once i decided to do it:
  // - replace vector with deque - acrually dont
  // - invoke afterscan as funcion finalise - yes
  // - expand token groups on place in deque - ???
  // that is likely the best way - it is
  // maybe not - no maybees
  Token expand_macro(Position expand_pos, Tokeniser& tkz,
                     MacroStamp macroStamp) {
    const unsigned expand_idx = out.size() - 1;
    const indentos indent{std::cerr, true};
    std::string_view src = tkz.get_src();

    Token token = tkz.read_token();
    if (!macroStamp.info.is_functional) {
      // just add expansion and return
      // for now i just return next token as that is what expected
      out.pop_back();
      out.emplace_back(expand_pos, macroStamp.expansion);
      return token;
    }
    // here we skipping all extras, without writing them anywhere
    // if one need them for some reason, one should implement smarter dump logic
    // good luck
    while (tag::is_extra(token.tag)) token = tkz.read_token();
    if (token.tag != tag::raw('(')) return token;

    const unsigned arg_range_origin_idx = arg_rages.size();
    arg_rages.push_back(out.size());

    static constexpr unsigned balance_origin = 0;
    int balance = balance_origin;
    bool successfullness = false;
    while (token.tag != tag::eof) {
      // TODONOW: preserve at least one space
      if (tag::is_extra(token.tag)) {
        out.emplace_back(token.range.start_pos, " ");
        do {
          token = tkz.read_token();
        } while (tag::is_extra(token.tag));
        continue;
      }
      switch (token.tag) {
        case tag::raw('('): {
          ++balance;
          break;
        }
        case tag::raw(')'): {
          --balance;
          if (balance != balance_origin) break;
          // remember end position for last arg
          arg_rages.push_back(out.size());
          out.emplace_back(token, src);
          // setup for loop exit:
          successfullness = true;
          token.tag = tag::eof;
          continue;  // in fact break from while loop
        }

        case tag::raw(','): {
          if (balance == 1) arg_rages.push_back(out.size());
          break;
        }

        case tag::identifier: {
          const auto macro_name = token.get_text(src);
          auto macroIt = macroMap.find(macro_name);
          if (macroIt == macroMap.end()) break;
          if (std::find(macro_stack.begin(), macro_stack.end(), macro_name) !=
              macro_stack.end())
            break;

          out.emplace_back(token, src);
          macro_stack.push_back(macro_name);
          token = expand_macro(token.range.start_pos, tkz,
                               MacroStamp{macroIt->second});
          macro_stack.pop_back();
          continue;
        }

        default:
          break;
      }
      out.emplace_back(token, src);
      token = tkz.read_token();
    }

    const size_t nargs_input = arg_rages.size() - 1 - arg_range_origin_idx;
    if (successfullness && macroStamp.is_valid_call(nargs_input)) {
      // to this point we have whole bunch of expansion relevant tokens
      // also marked argument ranges
      // also argument count
      // what do we do with it?

      // we expand it into buffer

      const auto arg_range_origin = arg_rages.begin() + arg_range_origin_idx;
      MacroExpansionTokeniser macro_tkz{macroStamp.expansion};
      while (!macro_tkz.eof()) {
        token = macro_tkz.read_token();
        if (token.tag == tag::arg) {
          const size_t arg_idx = token.external_index;
          auto arg_it = out.begin() + arg_range_origin[arg_idx] + 1;
          const auto arg_end = out.begin() + arg_range_origin[arg_idx + 1];
          for (; arg_it != arg_end; ++arg_it) {
            buffer.push_back(*arg_it);
          }
          continue;
        }
        Position tok_pos = expand_pos;
        tok_pos.column += expand_pos.column;
        buffer.emplace_back(tok_pos, token.get_text(macroStamp.expansion));
      }

      // buffer.emplace_back(expand_pos, macroStamp.expansion);

      // then expand back to output on place of previous tokens
      out.erase(out.begin() + expand_idx, out.end());
      out.insert(out.end(), buffer.begin(), buffer.end());
      buffer.clear();
    }
    arg_rages.erase(arg_rages.begin() + arg_range_origin_idx,  //
                    arg_rages.end());
    // if cant expand
    // maybe find better match if such exists?
    // dump all the deque
    // but actually seems like just leave it as is and return next token

    return tkz.read_token();
  }

  const auto& process_code(std::string_view src) {
    out.clear();
    size_t h = 0;
    Tokeniser tkz = Tokeniser{src};
    Token lastExpansionToken;

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
          // printit(macro_name);
          // printit(compile_macro_expansion(tkz.defineImage, src));
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

          // out.clear();
          out.emplace_back(token, src);
          macro_stack.push_back(macro_name);
          token = expand_macro(token.range.start_pos, tkz,
                               MacroStamp{macroIt->second});
          macro_stack.pop_back();
          continue;
        }
        default:
          out.emplace_back(token, src);
          break;  // from switch
      }
      token = tkz.read_token();
    };

    once {
      notignore += out.size();
      printit(out.size());
      lastExpansionToken.print(src, std::cerr);
      auto exp = macroMap.at("CTIMEOPT_VAL");
      printit(exp);
    };
    return out;
  }
};

int perf_test() {
  std::string src = read_file(ROOT "pp.test/sqliteall.c");
  timeit;
  repeat(5) {
    repeat(20) {
      Preprocessor pre;
      pre.process_code(src);
    }
    untimeit;
    usleep(500'000);
  }
  return 0;
}

int user_test() {
  // std::string src = read_file(ROOT "pp.test/pp.in.cpp");
  std::string src = read_file(ROOT "pp.test/short.cpp");
  timeit;
  Preprocessor pre;
  for (const auto out_tok : pre.process_code(src)) {
    out_tok.print(std::cerr);
  }

  return 0;
}

int main(int argc, char* argv[]) {
  perf_test();
  // user_test();
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
  Preprocessor pre;
  pre.process_code(src);

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

testit(tokenise_macro_expansion) {
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
      std::string compile = compile_macro_expansion(tkz.defineImage, src);

      MacroExpansionTokeniser macro_tkz{compile};

      token = macro_tkz.read_token();
      while (token.tag != tag::eof) {
        token.print(compile, std::cerr);
        token = macro_tkz.read_token();
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