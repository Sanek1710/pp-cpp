

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

class RunTimer {
  inline static unsigned depth = 0;

 public:
  RunTimer(const char* name, unsigned line) : name(name), line(line) {
    start = Clock::now();
    ++depth;
  }
  ~RunTimer() {
    --depth;
    auto duration = Clock::now() - start;
    auto ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    std::cerr << std::setw(depth + 1) << "[" << std::setw(6) << ms << "] "
              << std::setw(3) << line << ": " << name << "\n";
  }

 private:
  const char* name;
  const unsigned line;

  TImePoint start;
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

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define CAT_HELPER(x, y) x##y
#define CAT(x, y) CAT_HELPER(x, y)

#define stimeit(name) const RunTimer CAT(_rt_, __LINE__)(name, __LINE__)
#define timeit stimeit(__func__)
#define checkin std::cerr << "[here]: " << __LINE__ << "\n"
#define printit(x) std::cerr << x << "\n"

#define stotaltimeit(name)                               \
  static TimeHolder CAT(_th_, __LINE__)(name, __LINE__); \
  const TotalRunTimer CAT(_trt_, __LINE__) { (CAT(_th_, __LINE__)) }
#define totaltimeit stotaltimeit(__func__)
