
#include <iostream>
#include <vector>

#include "ExpansionTokeniser.h"
#include "MacroStamp.h"
#include "Preprocessor.h"
#include "tkz/Token.h"
#include "util/helper.h"
#include "util/ranges.h"
#include "util/util.h"

testit(range) {
  std::vector<int> vec = {1, 2, 3, 4, 5};
  Range range1(vec);  
  for (auto x : range1) {
    std::cout << x << " ";
  }
  std::cout << "\n";

  
  Range range2(vec.begin() + 1, vec.end() - 1);  
  for (auto x : range2) {
    std::cout << x << " ";
  }
  std::cout << "\n";

  
  std::array<double, 3> arr = {1.1, 2.2, 3.3};
  Range range3(arr);  
  for (auto x : range3) {
    std::cout << x << " ";
  }
  std::cout << "\n";

  
  std::cout << "range1 size: " << range1.size() << "\n";
  std::cout << "range2 size: " << range2.size() << "\n";
  std::cout << "range1 empty: " << range1.empty() << "\n";
}

testit(slice) {
  std::vector<int> vec = {1, 2, 3, 4, 5};
  IndexRange slice1(vec);  
  for (auto x : slice1) {
    std::cout << x << " ";
  }
  std::cout << "\n";

  IndexRange slice2(vec, 1, vec.size() - 1);  
  for (auto x : slice2) {
    std::cout << x << " ";
  }
  std::cout << "\n";

  
  std::array<double, 3> arr = {1.1, 2.2, 3.3};
  IndexRange slice3(arr);  
  for (auto x : slice3) {
    std::cout << x << " ";
  }
  std::cout << "\n";

  
  std::cout << "slice1 size: " << slice1.size() << "\n";
  std::cout << "slice2 size: " << slice2.size() << "\n";
  std::cout << "slice1 empty: " << slice1.empty() << "\n";
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

  Tokeniser tkz{src};
  Token token = tkz.read_token();
  while (token.tag != tag::eof) {
    if (token.tag == tag::pp_define) {
      const DefineView& defineImage = tkz.tokenImage().as_define();
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

  Tokeniser tkz{src};
  Token token = tkz.read_token();
  while (token.tag != tag::eof) {
    if (token.tag == tag::pp_define) {
      const DefineView& defineImage = tkz.tokenImage().as_define();
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

untestit(preprocess_sqlite) {
  timeit;
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
  std::string src = read_file(ROOT "pp.test/test.cpp");
  std::string pp_exp = read_file(ROOT "pp.test/act.pp.test.cpp");
  std::string pp_act;
  Preprocessor pre;
  pre.process_code(src, pp_act);
  check_result_print(pp_act, pp_exp);
  write_file(ROOT "pp.test/last.pp.test.cpp", pp_act);
}



int main(int argc, char *argv[]) {
  std::cout << "tests"
            << "\n";
  return 0;
}