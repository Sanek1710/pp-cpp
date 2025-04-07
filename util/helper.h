#pragma once

#include <iostream>

#include "ctrl.h"
#include "indentos.h"
#include "macro.h"
#include "rwfile.h"
#include "testit.h"
#include "timer.h"

#define checkin std::cerr << "[here]: " << __LINE__ << "\n"
#define printit(x) std::cerr << #x ": " << (x) << "\n"

#define repeat(n) for (size_t _i = n; _i; --_i)
#define once for (static bool _done = false; !_done; _done = true)
#define never for (; false;)

#define scval static constexpr auto 

#define ostreamop(T, name) \
  std::ostream& operator<<(std::ostream& os, const T& name)

#define deadnote(type, name)                                            \
  class DeadNote##name {                                                \
   public:                                                              \
    DeadNote##name() {}                                                 \
    ~DeadNote##name() {                                                 \
      std::cerr << std::setw(15) << #name ": " << std::setw(15) << name \
                << "\n";                                                \
    }                                                                   \
    inline static type name{};                                          \
  };                                                                    \
  DeadNote##name _deadNote##name {}

#define getnote(name) (DeadNote##name::name)

#define valacer(val, ntimes)   \
  static decltype(val) prev{}; \
  static size_t cntr = 0;      \
  if (prev == (val))           \
    ++cntr;                    \
  else {                       \
    prev = (val);              \
    cntr = 0;                  \
  }                            \
  if (cntr > ntimes)

class {
  struct NotIgnoreAdder {
    ~NotIgnoreAdder() { std::cerr << "\n\e[90mignore: " << val << "\e[0m\n"; }
    mutable int val = 0;
  } val;

 public:
  inline void operator+=(int other) const { val.val += other; }
} inline const notignore;
