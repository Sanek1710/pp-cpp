#pragma once

#include <array>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>

inline std::string read_file(const std::string filename) {
  std::ifstream ifs{filename};
  return std::string{std::istreambuf_iterator<char>{ifs},
                     std::istreambuf_iterator<char>{}};
}

inline void write_file(const std::string filename, std::string_view s) {
  std::ofstream ofs{filename};
  ofs.write(s.data(), s.size());
}

using Clock = std::chrono::system_clock;
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
  long ms = 0;
  TimeHolder(const char* name, unsigned line) : name(name), line(line) {}
  ~TimeHolder() {
    std::cerr << "#[" << std::setw(6) << ms << "] " << std::setw(3) << line
              << ": " << name << "\n";
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
    auto ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    timeHolder.ms += ms;
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

inline std::string appendUTF8(int code, std::string& out) {
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
  return out;
}
}  // namespace details

struct ctrl_str {
  ctrl_str(std::string_view sv) : sv(sv){};
  ctrl_str(const char& sv) : sv(&sv, 1){};
  std::string_view sv;
};
inline std::ostream& operator<<(std::ostream& os, const ctrl_str& ctrls) {
  std::string out;
  for (unsigned char c : ctrls.sv)
    details::appendUTF8(details::ctrl_encode(c), out);
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
#define printit(x) std::cerr << x << "\n"
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
