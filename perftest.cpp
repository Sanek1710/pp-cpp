#include <malloc.h>
#include <unistd.h>

#include <chrono>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <unordered_set>

#include "MacroStorage.h"

// Helper to measure memory
size_t getCurrentRSS() {
  // Note: this is Linux-specific. For Windows would need different
  // implementation
  FILE* fp = fopen("/proc/self/statm", "r");
  if (fp == nullptr) return 0;

  long rss = 0;
  if (fscanf(fp, "%*s%ld", &rss) != 1) {
    fclose(fp);
    return 0;
  }
  fclose(fp);
  return rss * sysconf(_SC_PAGESIZE);
}

// Test data generator
struct TestData {
  static constexpr size_t NUM_FILES = 500'000;
  static constexpr size_t MACROS_PER_FILE = 4;  // Average 4 macros per file
  static constexpr size_t TOTAL_MACROS = 2'000'000;

  std::mt19937 rng;
  std::uniform_int_distribution<> name_len{5, 50};
  std::uniform_int_distribution<> content_len{10, 200};
  std::uniform_int_distribution<FileID> file_dist{0, NUM_FILES - 1};

  TestData(uint32_t seed = 12345) : rng(seed) {}

  std::vector<std::pair<MacroName, MacroExpansion>> generateMacros(
      size_t count) {
    std::vector<std::pair<MacroName, MacroExpansion>> result;
    result.reserve(count);

    for (size_t i = 0; i < count; ++i) {
      MacroName name = "MACRO_" + std::to_string(rng() % 1000000);
      MacroExpansion exp{std::string(content_len(rng), 'x')};
      result.emplace_back(name, exp);
    }
    return result;
  }

  Set<FileID> generateIncludes(size_t count) {
    Set<FileID> includes;
    includes.reserve(count);
    for (size_t i = 0; i < count; ++i) {
      includes.insert(file_dist(rng));
    }
    return includes;
  }
};

// Performance tester
class PerformanceTester {
  using Clock = std::chrono::high_resolution_clock;
  using Duration = std::chrono::duration<double>;

 public:
  void runTests(uint32_t seed = 12345) {
    MacroStorage storage;
    TestData generator(seed);

    size_t initial_memory = getCurrentRSS();
    std::cout << "Initial memory: " << (initial_memory / 1024 / 1024)
              << "MB\n\n";

    // Test 1: Insert macros
    std::cout << "Testing macro insertion...\n";
    auto start = Clock::now();

    std::vector<MacroName> stored_names;
    size_t minMemory = 0;
    for (FileID fid = 0; fid < TestData::NUM_FILES; ++fid) {
      auto macros = generator.generateMacros(TestData::MACROS_PER_FILE);
      for (const auto& m : macros) {
        minMemory += memory(m.first) + memory(m.second.content);
        stored_names.push_back(m.first);
      }
      storage.insertFileMacros(fid, macros);
    }
    size_t memoryStoredNames = memory(stored_names);
    initial_memory -= memoryStoredNames;

    auto insert_time =
        std::chrono::duration_cast<Duration>(Clock::now() - start);
    size_t memory_after_insert = getCurrentRSS();
    if (malloc_trim(0)) std::cout << "trimmed\n";
    size_t memory_after_insert_after_trim = getCurrentRSS();

    std::cout << "Insertion time: " << insert_time.count() << "s\n";
    std::cout << "Memory used: "
              << ((memory_after_insert - initial_memory) / 1024 / 1024)
              << "MB\n\n";
    std::cout << "Memory used: (after trim)"
              << ((memory_after_insert_after_trim - initial_memory) / 1024 /
                  1024)
              << "MB\n\n";
    std::cout << "Min Memory Limit: " << ((minMemory) / 1024 / 1024)
              << "MB\n\n";

    // Test 2: Lookup performance
    std::cout << "Testing macro lookup...\n";
    start = Clock::now();

    size_t found_count = 0;
    size_t lookup_count = 1000000;
    for (size_t i = 0; i < lookup_count; ++i) {
      auto includes = generator.generateIncludes(50);  // simulate 50 includes
      auto& name = stored_names[i % stored_names.size()];
      if (storage.findMacro(name, includes) != nullptr) {
        found_count++;
      }
    }

    auto lookup_time =
        std::chrono::duration_cast<Duration>(Clock::now() - start);
    std::cout << "Lookup time: " << lookup_time.count() << "s\n";
    std::cout << "Looked up macros: " << lookup_count << "\n";
    std::cout << "Per lookup: " << (lookup_time.count() / lookup_count) << "\n";
    std::cout << "Found macros: " << found_count << "\n\n";
    storage.memstats(std::cout);

    // Test 3: Update performance
    std::cout << "Testing file updates...\n";
    start = Clock::now();

    for (size_t i = 0; i < 1000; ++i) {
      FileID fid = i % TestData::NUM_FILES;
      auto macros = generator.generateMacros(TestData::MACROS_PER_FILE);
      // storage.updateFileMacros(fid, macros);
    }

    auto update_time =
        std::chrono::duration_cast<Duration>(Clock::now() - start);
    size_t final_memory = getCurrentRSS();

    std::cout << "Update time: " << update_time.count() << "s\n";
    std::cout << "Final memory: "
              << ((final_memory - initial_memory) / 1024 / 1024) << "MB\n";
  }
};

int main() {
  PerformanceTester tester;
  tester.runTests(12345);  // or any other seed you want
  return 0;
}