#include <unistd.h>

#include <iostream>
#include <string>
#include <tuple>
#include <unordered_map>

#include "CompiledMacro.h"
#include "Preprocessor.h"
#include "TokenPrinter.h"
#include "tkz/TokenGroup.h"
#include "util/helper.h"
#include "util/util.h"

struct TestCase {
  virtual int run() const = 0;
  inline static std::unordered_map<std::string, TestCase*> cases;
  static int run(std::string name) {
    auto cases_it = cases.find(name);
    if (cases_it == cases.end()) {
      std::cerr << "no such test: " << name;
      return -1;
    }
    return cases_it->second->run();
  }
  static int run_all() {
    int status = 0;
    for (const auto [test_name, test_case] : cases) {
      int test_status = test_case->run();
      if (test_status != 0) {
        std::cerr << "[FAIL] " << test_name << "\n";
        status = test_status;
      }
    }
    return status;
  }
};

#define test(name)                                      \
  struct TestCase##name : TestCase {                    \
    TestCase##name() { TestCase::cases[#name] = this; } \
    int run() const;                                    \
  } static const TestCase##name;                        \
  int TestCase##name::run() const

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

test(compile_macro_expansion) {
  // return;
  std::string src = read_file(ROOT "tests/input/various-macros.cpp");

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
  size_t nfailed = 0;
  while (token.tag != tag::eof) {
    if (token.tag == tag::pp_define) {
      const DefineView& defineImage = tkz.tokenImage().as_define();
      std::string_view name = defineImage.name().get_text();
      std::string act = CompiledMacro{defineImage}.take();
      defineImage.print(std::cerr);
      std::cerr << "\n";

      if (expected.contains(name)) {
        auto exp = expected.at(name);
        if ((act) != (exp)) {
          ++nfailed;
          std::cerr << "\033[31m[fail]\033[0m  act: `" << (act) << "`\n";
          std::cerr << "\033[31m      \033[0m  exp: `" << (exp) << "`\n";
        }
      } else {
        std::cerr << "\033[33m[skip]\033[0m  act: `" << (act) << "`\n";
      }
      std::cerr << "\n";
    }
    token = tkz.read_token();
  }
  return nfailed;
}

test(tokenise_macro_expansion) {
  // return;
  std::string src = read_file(ROOT "tests/input/various-macros.cpp");

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
      auto compile = CompiledMacro{defineImage};

      CompiledMacroTokeniser macro_tkz{compile.get_stamp()};

      token = macro_tkz.read_token();
      while (token.tag != tag::eof) {
        token.print(std::cerr);
        token = macro_tkz.read_token();
      }
      std::cerr << "\n";
    }
    token = tkz.read_token();
  }
  return 0;
}

int main(int argc, char* argv[]) {
  if (argc < 2) return TestCase::run_all();
  return TestCase::run(argv[1]);
}
