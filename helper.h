

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

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define CAT_HELPER(x, y) x##y
#define CAT(x, y) CAT_HELPER(x, y)

#define timeit const RunTimer CAT(_rt_, __LINE__)(__func__, __LINE__);
#define checkin std::cerr << "[here]: " << __LINE__ << "\n";
#define printit(x) std::cerr << x << "\n";
