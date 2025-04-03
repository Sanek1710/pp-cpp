#include <unistd.h>

#include "Cursor.h"
#include "TokenGroup.h"
#include "TokenPrinter.h"
#include "preprocess.h"

int perf_test() {
  std::string src = read_file(ROOT "pp.test/sqliteall.c");
  std::string out;
  timeit;
  repeat(5) {
    repeat(20) {
      Preprocessor pre;
      pre.process_code(src, out);
    }
    untimeit;
    usleep(500'000);
  }
  return 0;
}

void print_source(std::string_view src) {
  DirectiveTokenImage ti;
  Tokeniser tkz{src, ti};
  TokenPrinter printer{std::cerr, true};
  while (!tkz.eof()) printer.print(tkz.read_token());
}

int user_test() {
  std::string src = read_file(ROOT "pp.test/pp.in.cpp");
  // std::string src = read_file(ROOT "helper.h");
  // src += read_file(ROOT "preprocess.cpp");
  timeit;
  Preprocessor pre;
  TokenPrinter printer{std::cerr, true};
  std::string out;
  for (const auto out_tok : pre.process_code(src, out)) {
    // printer.print(out_tok);
    // out_tok.print(std::cerr);
    // std::cerr << "\n";
  }
  print_source(out);

  std::cerr << "\n\n";

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
  Preprocessor pre;
  pre.process_code(src);

  return 0;
}

untestit(compile_macro_expansion) {
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

  DirectiveTokenImage tokenImage;
  Tokeniser tkz{src, tokenImage};
  Token token = tkz.read_token();
  while (token.tag != tag::eof) {
    if (token.tag == tag::pp_define) {
      const DefineTokenImage& defineImage = tokenImage.as<DefineTokenImage>();
      std::string_view name = defineImage.name().get_text();
      std::string act = compile_macro_expansion(defineImage);
      defineImage.print(std::cerr);
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

untestit(tokenise_macro_expansion) {
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

  DirectiveTokenImage tokenImage;
  Tokeniser tkz{src, tokenImage};
  Token token = tkz.read_token();
  while (token.tag != tag::eof) {
    if (token.tag == tag::pp_define) {
      const DefineTokenImage& defineImage = tokenImage.as<DefineTokenImage>();
      std::string_view name = defineImage.name().get_text();
      std::string compile = compile_macro_expansion(defineImage);

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