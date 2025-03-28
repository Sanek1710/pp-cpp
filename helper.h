#pragma once

#include <array>
#include <cctype>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>

using namespace std::chrono_literals;

inline std::string read_file(const std::string filename) {
  std::ifstream ifs{filename};
  return std::string{std::istreambuf_iterator<char>{ifs},
                     std::istreambuf_iterator<char>{}};
}

inline void write_file(const std::string filename, std::string_view s) {
  std::ofstream ofs{filename};
  ofs.write(s.data(), s.size());
}

using Clock = std::chrono::high_resolution_clock;
using TImePoint = Clock::time_point;

class BaseTimer {
 public:
  BaseTimer() : start(Clock::now()) {}
  ~BaseTimer() {}

  long elapsed() const {
    auto duration = Clock::now() - start;
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration)
        .count();
  }

 private:
  TImePoint start;
};

class RunTimer : BaseTimer {
  inline static unsigned depth = 0;
  inline static std::array<RunTimer*, 10> instances;

  struct Pauser : BaseTimer {
    Pauser(RunTimer& runTimer) : runTimer(runTimer) {}
    ~Pauser() { runTimer.paused_ms += elapsed(); }
    RunTimer& runTimer;
  };

 public:
  RunTimer(const char* name, unsigned line) : name(name), line(line) {
    ++depth;
    instances[depth] = this;
    std::cerr << "> " << std::setw(3) << line << ": " << name << "\n";
  }
  ~RunTimer() {
    --depth;
    auto ms = elapsed() - paused_ms;
    std::cerr << std::setw(depth + 1) << "[" << std::setw(6) << ms << "] "
              << std::setw(3) << line << ": " << name << "\n";
  }

  Pauser pauseLifetime() { return *this; }
  static RunTimer& last_instance() { return *instances[depth]; }

 private:
  const char* name;
  const unsigned line;
  long paused_ms = 0;
};

class TimeHolder {
 public:
  Clock::duration duration = 0ms;

  TimeHolder(const char* name, unsigned line) : name(name), line(line) {}
  ~TimeHolder() {
    auto ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
    std::cerr << "#[" << std::setw(6) << (ns / 1'000'000) << "] "
              << std::setw(3) << line << ": " << name << "\n";
  }

 private:
  const char* name;
  const unsigned line;
};

class TotalRunTimer {
  inline static unsigned depth = 0;

 public:
  TotalRunTimer(TimeHolder& timeHolder) : timeHolder(timeHolder) {
    start = Clock::now();
  }
  ~TotalRunTimer() {
    auto duration = Clock::now() - start;
    timeHolder.duration += duration;
  }

 private:
  TimeHolder& timeHolder;
  TImePoint start;
};

namespace details {

inline int ctrl_encode(unsigned char c) {
  if (c == 0 ||  //
      '\t' <= c && c <= '\r')
    return 224 | c;
  return c;
}

inline auto& appendCtrlEncode(int code, std::string& out) {
  if (!std::iscntrl(code)) {
    out += code;
    return out;
  }

  switch (code) {
    case '\0':
      return out += "\\0";
    case '\a':
      return out += "\\a";
    case '\b':
      return out += "\\b";
    case '\t':
      return out += "\\t";
    case '\n':
      return out += "\\n";
    case '\v':
      return out += "\\v";
    case '\f':
      return out += "\\f";
    case '\r':
      return out += "\\r";
    default:
      return out += "\\.";
  }
}

inline void appendUTF8(int code, std::string& out) {
  if (code < 128) {
    out += static_cast<char>(code);
  } else if (code < 2048) {
    out += static_cast<char>((code >> 6) | 192);
    out += static_cast<char>((code & 63) | 128);
  } else if (code < 65536) {
    out += static_cast<char>((code >> 12) | 224);
    out += static_cast<char>(((code >> 6) & 63) | 128);
    out += static_cast<char>((code & 63) | 128);
  }
}
}  // namespace details

struct ctrl_str {
  ctrl_str(std::string_view sv) : sv(sv){};
  ctrl_str(char c) : letter(c), sv(&letter, 1){};
  std::string_view sv;
  char letter;
};
inline std::ostream& operator<<(std::ostream& os, const ctrl_str& ctrls) {
  std::string out;
  for (unsigned char c : ctrls.sv) {
    // details::appendUTF8(details::ctrl_encode(c), out);
    details::appendCtrlEncode(c, out);
  }
  return os << out;
}

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define CAT_HELPER(x, y) x##y
#define CAT(x, y) CAT_HELPER(x, y)

#define suntimeit(line) \
  const auto CAT(_rut_, __LINE__) { RunTimer::last_instance().pauseLifetime() }
#define untimeit suntimeit(__func__)

#define stimeit(name) \
  const RunTimer CAT(_rt_, __LINE__) { name, __LINE__ }
#define timeit stimeit(__func__)
#define checkin std::cerr << "[here]: " << __LINE__ << "\n"
#define printit(x) std::cerr << #x ": " << (x) << "\n"
#define repeat(n) for (size_t _i = n; _i; --_i)

#define stotaltimeit(name)                               \
  static TimeHolder CAT(_th_, __LINE__)(name, __LINE__); \
  const TotalRunTimer CAT(_trt_, __LINE__) { (CAT(_th_, __LINE__)) }
#define totaltimeit stotaltimeit(__func__)

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

#define once for (static bool _done = false; !_done; _done = true)
#define never for (; false;)

#define WOW(...) \
  do it now __VA_ARGS__
#define CATTER(ab) #ab##cd
#define CATTER2(ab) L#ab

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

#define testit(name, ...)                                             \
  class LifetimeTest##name {                                          \
   public:                                                            \
    LifetimeTest##name() {                                            \
      runwrapper();                                                   \
      __VA_OPT__(exit(0));                                            \
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
  const static LifetimeTest##name _lttst##name;                       \
  inline void LifetimeTest##name::run()

class NotIgnoreAdder {
 public:
  ~NotIgnoreAdder() { std::cerr << "\n\e[90mignore: " << val << "\e[0m\n"; }
  inline void operator+=(int other) const { val += other; }
  mutable int val = 0;
} inline const notignore;

#ifndef INDENT_OS_H_
#define INDENT_OS_H_

#include <iostream>

class indentos : public std::streambuf {
  std::streambuf* rdbuf;
  bool newline = false;
  std::ostream& os;
  inline static const std::string_view indent = "  ";

 protected:
  virtual int overflow(int ch) {
    if (newline && ch != '\n') rdbuf->sputn(indent.data(), indent.size());
    newline = ch == '\n';
    return rdbuf->sputc(ch);
  }

 public:
  explicit indentos(std::ostream& os, bool newline = false)
      : rdbuf(os.rdbuf()), newline(newline), os(os) {
    os.rdbuf(this);
  }
  virtual ~indentos() { os.rdbuf(rdbuf); }
};

#endif