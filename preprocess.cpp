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
#include <iterator>
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

class MacroExpansionTokeniser : private Tokeniser {
 public:
  MacroExpansionTokeniser(std::string_view src) : Tokeniser(src) {}

  inline Token read_token() {
    Token token;
    token.start = cur.it;
    token.start_pos = cur.to_position();
    token.tag = skip_next();
    if (token.tag == tag::arg) {
      cur.it = std::from_chars(cur.it, end, token.details.index).ptr;
      cur.it++;
    }
    token.size = cur.it - token.start;
    token.end_pos = cur.to_position();
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
  template <typename T>
  static inline void reset(std::vector<T>& container, size_t head) {
    container.erase(container.begin() + head, container.end());
  }
  StringMap<std::string> macroMap;

  std::vector<Token> out;
  std::vector<Token> buffer;
  std::vector<size_t> arg_rages;

  std::string str_buffer;
  std::vector<std::string_view> macro_stack;

  inline std::pair<std::vector<Token>::iterator, std::vector<Token>::iterator>
  arg_range(size_t arg_range_head, size_t arg_idx) {
    const auto arg_range_origin = arg_rages.begin() + arg_range_head + arg_idx;
    return {out.begin() + arg_range_origin[0] + 1,
            out.begin() + arg_range_origin[1]};
  }

  class TokeniserInterface {
   public:
    TokeniserInterface(std::vector<Token>& tokens, size_t begin_idx,
                       size_t end_idx)
        : tokens(tokens), idx(begin_idx), end_idx(end_idx) {}
    inline Token read_token() {
      if (idx == end_idx) return Token{.tag = tag::eof};
      return tokens[idx++];
    }

   private:
    size_t idx;
    const size_t end_idx;
    std::vector<Token>& tokens;
  };

 public:
  template <typename TokeniserT>
  Token process_macro_call(Token macro_token, TokeniserT& tkz,
                           MacroStamp macroStamp) {
    const unsigned expand_head = out.size();
    const unsigned arg_range_head = arg_rages.size();

    if (!macroStamp.info.is_functional) {
      expand_macro(macro_token, macroStamp, expand_head, arg_range_head);
      return tkz.read_token();
    }
    // here we skipping all extras, without writing them anywhere
    // if one need them for some reason, one should implement smarter dump logic
    // good luck
    out.push_back(macro_token);

    Token token = tkz.read_token();
    while (tag::is_extra(token.tag)) token = tkz.read_token();

    if (token.tag != tag::raw('(')) return token;

    arg_rages.push_back(out.size());

    static constexpr unsigned balance_origin = 0;
    int balance = balance_origin;

    while (token.tag != tag::eof) {
      // TODONOW: preserve at least one space
      if (tag::is_extra(token.tag)) {
        out.push_back(code_token(" ", token.start_pos));
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
          out.push_back(token);
          // god forgive me
          goto process_expansion;
        }

        case tag::raw(','): {
          if (balance == balance_origin + 1) arg_rages.push_back(out.size());
          break;
        }

        case tag::identifier: {
          const auto macro_name = token.get_text();
          auto macroIt = macroMap.find(macro_name);
          if (macroIt == macroMap.end()) {
            token.details.marker = true;
            break;
          }
          token.details.marker = false;

          if (std::find(macro_stack.begin(), macro_stack.end(), macro_name) !=
              macro_stack.end())
            break;

          macro_stack.push_back(macro_name);
          token = process_macro_call(token, tkz, MacroStamp{macroIt->second});
          macro_stack.pop_back();
          continue;
        }

        default:
          break;
      }
      out.push_back(token);
      token = tkz.read_token();
    }

  process_expansion:

    const size_t nargs_input = arg_rages.size() - 1 - arg_range_head;
    if (token.tag != tag::eof && macroStamp.is_valid_call(nargs_input)) {
      // to this point we have whole bunch of expansion relevant tokens
      // also marked argument ranges
      // also argument count
      // what do we do with it?

      // we expand it into buffer

      expand_macro(macro_token, macroStamp, expand_head, arg_range_head);
    } else {
      reset(arg_rages, arg_range_head);
    }
    return tkz.read_token();
  }
  void expand_macro(Token macro_token, MacroStamp macroStamp,
                    const size_t expand_head, const size_t arg_range_head) {
    const size_t buffer_head = buffer.size();
    // TODO: support base position for tokenisers
    MacroExpansionTokeniser macro_tkz{macroStamp.expansion};
    while (!macro_tkz.eof()) {
      Token token = macro_tkz.read_token();
      if (token.tag == tag::arg) {
        const size_t arg_idx = token.details.index;
        auto [arg_it, arg_end] = arg_range(arg_range_head, arg_idx);
        for (; arg_it != arg_end; ++arg_it) {
          buffer.push_back(*arg_it);
        }
        continue;
      }
      token.start_pos.line = macro_token.start_pos.line;
      token.start_pos.column += macro_token.start_pos.column;
      token.end_pos.line += macro_token.start_pos.line;
      token.end_pos.column += macro_token.start_pos.column;
      buffer.push_back(token);
    }

    reset(arg_rages, arg_range_head);
    reset(out, expand_head);

    // TODO: treat bufer as new input to read from
    // then expand back to output on place of previous tokens
    // out.insert(out.end(), buffer.begin() + buffer_head, buffer.end());
    post_process_expansion(buffer_head);

    reset(buffer, buffer_head);
  }

  const auto& process_code(std::string_view src) {
    out.clear();
    buffer.clear();
    arg_rages.clear();

    Tokeniser tkz = Tokeniser{src};
    Token token = tkz.read_token();
    while (token.tag != tag::eof) {
      switch (token.tag) {
        case tag::pp_include:
          // totaltimeit;
          break;

        case tag::pp_define:
          macroMap.emplace(tkz.defineImage.name.get_text(),
                           compile_macro_expansion(tkz.defineImage, src));
          break;

        case tag::pp_undef:
          macroMap.erase(tkz.undefImage.name.get_text());
          break;

        case tag::identifier: {
          // break;
          const auto macro_name = token.get_text();
          auto macroIt = macroMap.find(macro_name);
          if (macroIt == macroMap.end()) {
            token.details.marker = true;
            break;
          }
          token.details.marker = false;

          // out.clear();
          macro_stack.push_back(macro_name);
          token = process_macro_call(token, tkz, MacroStamp{macroIt->second});
          macro_stack.pop_back();
          continue;
        }
        default:
          break;  // from switch
      }
      // out.push_back(token);
      token = tkz.read_token();
    };

    once {
      notignore += out.size();
      printit(out.size());
    };
    return out;
  }

  inline void post_process_expansion(size_t buffer_head) {
    TokeniserInterface tkz{buffer, buffer_head, buffer.size()};
    Token token = tkz.read_token();
    while (token.tag != tag::eof) {
      switch (token.tag) {
        case tag::identifier: {
          const auto macro_name = token.get_text();
          auto macroIt = macroMap.find(macro_name);
          if (macroIt == macroMap.end()) {
            token.details.marker = true;
            break;  // from switch
          }
          token.details.marker = false;

          if (std::find(macro_stack.begin(), macro_stack.end(), macro_name) !=
              macro_stack.end())
            break;

          macro_stack.push_back(macro_name);
          token = process_macro_call(token, tkz, MacroStamp{macroIt->second});
          macro_stack.pop_back();
          continue;
        }
        default:
          break;
      }
      out.push_back(token);
      token = tkz.read_token();
    }
  }

 private:
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
  std::string src = read_file(ROOT "helper.h");
  src += read_file(ROOT "preprocess.cpp");
  timeit;
  Preprocessor pre;
  TokenPrinter printer{std::cerr, true};
  for (const auto out_tok : pre.process_code(src)) {
    printer.print(out_tok);
    // out_tok.print(std::cerr);
    // std::cerr << "\n";
  }
  std::cerr << "\n\n";

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
  std::string src = read_file(ROOT "pp.test/pp.in.cpp");

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
  while (token.tag != tag::eof) {
    if (token.tag == tag::pp_define) {
      std::string_view name = tkz.defineImage.name.get_text();
      std::string act = compile_macro_expansion(tkz.defineImage, src);
      tkz.defineImage.print(std::cerr);
      std::cerr << "\n";

      if (expected.contains(name)) {
        auto exp = expected.at(name);
        check_print(act, exp);
      } else {
        uncheck_print(act);
      }
      std::cerr << "\n";
    }
    token = tkz.read_token();
  }
  // exit(0);
}

testit(tokenise_macro_expansion) {
  // return;
  std::string src = read_file(ROOT "pp.test/pp.in.cpp");

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
  while (token.tag != tag::eof) {
    if (token.tag == tag::pp_define) {
      std::string_view name = tkz.defineImage.name.get_text();
      std::string compile = compile_macro_expansion(tkz.defineImage, src);

      MacroExpansionTokeniser macro_tkz{compile};

      token = macro_tkz.read_token();
      while (token.tag != tag::eof) {
        token.print(std::cerr);
        token = macro_tkz.read_token();
      }
      std::cerr << "\n";
    }
    token = tkz.read_token();
  }
  // exit(0);
}