#pragma once

#include <array>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>

#include "macro.h"

using namespace std::chrono_literals;

static std::ofstream timelogs =
    std::ofstream(ROOT "logs.tmp/time.log", std::ios_base::app);

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
    if (std::string_view(name) == "perf_test") timelogs << ms << "\n";
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

#define suntimeit(line) \
  const auto CAT(_rut_, __LINE__) { RunTimer::last_instance().pauseLifetime() }
#define untimeit suntimeit(__func__)

#define stimeit(name) \
  const RunTimer CAT(_rt_, __LINE__) { name, __LINE__ }
#define timeit stimeit(__func__)

#define stotaltimeit(name)                               \
  static TimeHolder CAT(_th_, __LINE__)(name, __LINE__); \
  const TotalRunTimer CAT(_trt_, __LINE__) { (CAT(_th_, __LINE__)) }
#define totaltimeit stotaltimeit(__func__)
