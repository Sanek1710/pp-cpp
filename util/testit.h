#pragma once

#include "macro.h"

#define check_print(act, exp)                                         \
  do {                                                                \
    if ((act) == (exp)) {                                             \
      std::cerr << "\033[32m[pass]\033[0m  act: `" << (act) << "`\n"; \
      std::cerr << "\033[32m      \033[0m  exp: `" << (exp) << "`\n"; \
    } else {                                                          \
      ++nfailed;                                                      \
      std::cerr << "\033[31m[fail]\033[0m  act: `" << (act) << "`\n"; \
      std::cerr << "\033[31m      \033[0m  exp: `" << (exp) << "`\n"; \
    }                                                                 \
  } while (false);

#define uncheck_print(act)                                          \
  do {                                                              \
    std::cerr << "\033[33m[skip]\033[0m  act: `" << (act) << "`\n"; \
  } while (false);

#define check_result_print(act, exp)                                   \
  do {                                                                 \
    if ((act) == (exp)) {                                              \
      std::cerr << "\033[32m[pass]\033[0m  act: `" << (#act) << "`\n"; \
      std::cerr << "\033[32m      \033[0m  exp: `" << (#exp) << "`\n"; \
    } else {                                                           \
      ++nfailed;                                                       \
      std::cerr << "\033[31m[fail]\033[0m  act: `" << (#act) << "`\n"; \
      std::cerr << "\033[31m      \033[0m  exp: `" << (#exp) << "`\n"; \
    }                                                                  \
  } while (false);

#define testit_base(name, onend, modifier)                            \
  class LifetimeTest##name {                                          \
   public:                                                            \
    LifetimeTest##name() {                                            \
      runwrapper();                                                   \
      onend;                                                          \
    }                                                                 \
    void runwrapper() {                                               \
      std::cerr << ("test::" #name) << " \\\n";                       \
      run();                                                          \
      std::cerr << ("test::" #name) << " /\n\n";                      \
      if (nfailed) {                                                  \
        std::cerr << "\033[31m[failed:]\033[0m: " << nfailed << "\n"; \
      }                                                               \
    }                                                                 \
    size_t nfailed = 0;                                               \
    void run();                                                       \
  };                                                                  \
  modifier const LifetimeTest##name _lttst##name;                     \
  inline void LifetimeTest##name::run()

#define testit(name) testit_base(name, EMPTY, EMPTY)
#define testitexit(name) testit_base(name, exit(0), EMPTY)
#define untestit(name) testit_base(name, EMPTY, extern)
