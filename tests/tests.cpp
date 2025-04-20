#include <unistd.h>

#include <iostream>
#include <string>
#include <tuple>
#include <unordered_map>

#include "Preprocessor.h"
#include "tkz/TokenGroup.h"
#include "TokenPrinter.h"
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

test(sqlite) {
  indentos indos{std::cerr};
  std::string src = read_file(ROOT "pp.test/sqliteall.txt");
  std::string pp_exp = read_file(ROOT "pp.test/act.pp.sqliteall.txt");
  std::string pp_act;
  Preprocessor pre;
  pre.process_code(src, pp_act);
  write_file(ROOT "pp.test/last.pp.sqliteall.txt", pp_act);

  if (pp_act != pp_exp) return -1;
  return 0;
}

test(perf) {
  std::string src = read_file(TEST_DIR "pp.test/sqliteall.txt");
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
  Tokeniser tkz{src};
  TokenPrinter printer{std::cerr, true};
  while (!tkz.eof()) printer.print(tkz.read_token());
}

test(user) {
  std::string src = read_file(ROOT "pp.test/test.cpp");
  // std::string src = read_file(ROOT "helper.h");
  // src += read_file(ROOT "preprocess.cpp");
  timeit;
  Preprocessor pre;
  TokenPrinter printer{std::cerr, true};
  std::string out;
  pre.process_code(src, out);
  print_source(out);

  std::cerr << "\n\n";

  return 0;
}

int main(int argc, char* argv[]) {
  if (argc < 2) return TestCase::run_all();
  return TestCase::run(argv[1]);
}
