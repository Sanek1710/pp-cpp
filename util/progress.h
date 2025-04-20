#pragma once

#include <string>
#include "timer.h"

class ProgrssBar {
  // inline static std::string loading = "⠛⠙⠹⠸⠼⠴⠶⠦⠧⠇⠏⠋";
  inline static std::string loading =
      "##################################################"
      "--------------------------------------------------";
  inline static const size_t loading_size = loading.size() / 2;
  inline static const size_t mark_percentage = 100 / loading_size;

 public:
  ProgrssBar() { print("", ""); }
  ~ProgrssBar() { print("\r", "\n"); }

  void operator()(size_t current, size_t total) {
    if (total == 0) return;
    size_t percentage = 100 * (current + 1) / total;
    if (last_percentage >= percentage) return;
    last_percentage = percentage;
    print("\r", "");
  };

 private:
  size_t last_percentage = 0;
  BaseTimer timer;

  void print(const char* start, const char* end) {
    size_t nmarks = last_percentage / mark_percentage;
    std::string_view loading_view{loading.data() + loading_size - nmarks,
                                  loading_size};
    size_t ms = timer.elapsed();
    size_t est_ms_left = ms * 100 / (last_percentage + 1);
    std::cerr << start << "[" << loading_view << "] " << last_percentage << "% ("
              << ms << " ms / " << est_ms_left << " ms)" << end;
  }
};

class IncrementalProgrssBar : private ProgrssBar {
 public:
  void operator()(size_t total) {
    ++current;
    ProgrssBar::operator()(current, total);
  };

 private:
  size_t current = 0;
};