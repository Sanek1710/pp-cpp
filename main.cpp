#include <unistd.h>

#include "Cursor.h"
#include "TokenGroup.h"
#include "TokenPrinter.h"
#include "preprocess.h"
#include "util/helper.h"
#include "util/indentos.h"
#include "util/testit.h"

int perf_test() {
  std::string src = read_file(ROOT "pp.test/sqliteall.txt");
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

int small_test() {
  std::string src = read_file(ROOT "pp.test/small.cpp");
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
  std::string out;
  pre.process_code(src, out);

  return 0;
}

untestit(compile_macro_expansion) {
  // return;
  std::string src = read_file(ROOT "pp.test/pp.in.cpp");

  StringMap<std::string> expected =  //
      {
          {"TWO_WORDS", "f0 aa bb"},                 //
          {"STR_AFTER_TOKEN", "f1 aa $0s"},          //
          {"NAMED_VA", "v1 some($0a)"},              //
          {"UNNAMED_VA", "v1 some($0a)"},            //
          {"INCNAMED_VA", "v1 some(__VA_ARGS__)"},   //
          {"INCUNNAMED_VA", "v1 some(named)"},       //
          {"CNSTINBOOL", " constexpr inline bool"},  //
          {"farg", "f1 farg($0a)"},                  //
          {"f", "f0 f()"},                           //
          {"abeta", "f1 alpha##$0r##gamma"},         //
          {"SELF1", "f2 ($0a, $1a)"},                //
          {"CAT1", "f2 $0r##$1r"},                   //
          {"STR1", "f1 $0s"},                        //
          {"STR1", "f1 $0s"},                        //
          {"CATSTR1", "f2 $0a$1s"},                  //
          {"CATSTR2", "f2 $0r##$1s"},                //
          {"CATSTR3", "f2 $0r##$1s"},                //
          {"CATSTR4", "f2 $0s##$1s"},                //
          {"CATSTR5", "f2 $0r##$1r"},                //
          {"EMPTY", " "},                            //
          {"EMPTYF", "f0 "},                         //
          {"SELF2", "f2 $0a, $1a"},                  //
      };

  DirectiveTokenImage tokenImage;
  Tokeniser tkz{src, tokenImage};
  Token token = tkz.read_token();
  while (token.tag != tag::eof) {
    if (token.tag == tag::pp_define) {
      const DefineView& defineImage = tokenImage.as_define();
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
          {"NAMED_VA", "v1 some($0a)"},              //
          {"UNNAMED_VA", "v1 some($0a)"},            //
          {"INCNAMED_VA", "v1 some(__VA_ARGS__)"},   //
          {"INCUNNAMED_VA", "v1 some(named)"},       //
          {"CNSTINBOOL", " constexpr inline bool"},  //
          {"farg", "f1 farg($0a)"},                  //
          {"f", "f0 f()"},                           //
          {"abeta", "f1 alpha$0rgamma"},             //
          {"SELF1", "f2 ($0a, $1a)"},                //
          {"CAT1", "f2 $0r$1r"},                     //
          {"STR1", "f1 $0s"},                        //
          {"STR1", "f1 $0s"},                        //
          {"CATSTR1", "f2 $0a$1s"},                  //
          {"CATSTR2", "f2 $0r$1s"},                  //
          {"CATSTR3", "f2 $0r$1s"},                  //
          {"CATSTR4", "f2 $0s$1s"},                  //
          {"CATSTR5", "f2 $0r$1r"},                  //
      };

  DirectiveTokenImage tokenImage;
  Tokeniser tkz{src, tokenImage};
  Token token = tkz.read_token();
  while (token.tag != tag::eof) {
    if (token.tag == tag::pp_define) {
      const DefineView& defineImage = tokenImage.as_define();
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

untestit(compl ) {
  // static_assert(incompatible(tag::identifier, tag::identifier));

  const Tag tags[]{
      tag::raw('!'), tag::raw('.'),   tag::raw(':'),
      tag::raw('%'), tag::raw('&'),   tag::raw('*'),
      tag::raw('+'), tag::raw('-'),   tag::raw('/'),
      tag::raw('<'), tag::raw('='),   tag::raw('>'),
      tag::raw('|'), tag::raw('^'),   tag::raw('`'),
      tag::raw('('), tag::raw(')'),   tag::raw(','),
      tag::raw(';'), tag::raw('?'),   tag::raw('@'),
      tag::raw('['), tag::raw(']'),   tag::raw('{'),
      tag::raw('}'), tag::raw('~'),   tag::string_like_literal,
      tag::number,   tag::identifier,
  };
  for (auto l : tags) {
    for (auto r : tags) {
      // std::cerr << (incompatible(l, r) ? '+' : ' ') << "|";
    }
    std::cerr << "\n";
  }
}

testit(preprocess_sqlite) {
  indentos indos{std::cerr};
  std::string src = read_file(ROOT "pp.test/sqliteall.txt");
  std::string pp_exp = read_file(ROOT "pp.test/act.pp.sqliteall.txt");
  std::string pp_act;
  Preprocessor pre;
  pre.process_code(src, pp_act);
  check_result_print(pp_act, pp_exp);
  write_file(ROOT "pp.test/last.pp.sqliteall.txt", pp_act);
}
testit(preprocess_pptest) {
  indentos indos{std::cerr};
  std::string src = read_file(ROOT "pp.test/pp.test.cpp");
  std::string pp_exp = read_file(ROOT "pp.test/1act.pp.pp.test.cpp");
  std::string pp_act;
  Preprocessor pre;
  pre.process_code(src, pp_act);
  check_result_print(pp_act, pp_exp);
  write_file(ROOT "pp.test/2act.pp.pp.test.cpp", pp_act);
}

int main(int argc, char* argv[]) {
  user_test();
  perf_test();
  small_test();
}