#pragma once

#include <mutex>
#include <ostream>
#include <shared_mutex>
#include <string>

#include "FileID.h"
#include "MacroMap.h"
#include "util/bytesize.h"
#include "util/util.h"

inline std::ostream& operator<<(std::ostream& os, const MacroMap& macro_map) {
  for (const auto& [name, expansion] : macro_map) {
    os << "  " << name << ": " << expansion << "\n";
  }
  return os;
}

class Storage {

  struct MacroView {
    std::string_view name;
    std::string_view expansion;
  };
  using MacroViewList = std::vector<MacroView>;

 public:
  void insert(FileID fileid, const MacroMap& macro_map) {
    if (macro_map.empty()) return;
    const std::unique_lock<std::shared_mutex> lock{mutex};
    auto& entry = storage.emplace(fileid, MacroViewList{}).first->second;
    entry.clear();
    entry.reserve(macro_map.size());
    for (const auto& [name, expansion] : macro_map) {
      entry.push_back(MacroView{.name = emplace_string(name),
                                .expansion = emplace_string(expansion)});
    }
  }

  void get(FileID fileid, MacroMap& macro_map) const {
    const std::shared_lock<std::shared_mutex> lock{mutex};
    auto storage_it = storage.find(fileid);
    if (storage_it == storage.end()) return;
    for (const auto macro_view : storage_it->second) {
      macro_map.emplace(macro_view.name, macro_view.expansion);
    }
  }

  void erase(FileID file_id) {
    const std::unique_lock<std::shared_mutex> lock{mutex};
    storage.erase(file_id);
  }

  void print(std::ostream& os) const {
    const std::shared_lock<std::shared_mutex> lock{mutex};
    for (const auto& [file_id, macro_list] : storage) {
      os << "[" << file_id << "]:\n";
      for (const auto& macro_view : macro_list) {
        os << "  " << macro_view.name << ": " << macro_view.expansion << "\n";
      }
    }
  }

  ByteSize memory() const {
    const std::shared_lock<std::shared_mutex> lock{mutex};
    static const size_t sso_size = std::string{}.capacity();
    size_t total_size = 0;
    size_t nentries = storage.bucket_count() * storage.load_factor();
    total_size += nentries * sizeof(MacroView);
    for (const auto& str : strings) {
      total_size += sizeof(std::string);
      if (str.capacity() > sso_size) total_size += str.capacity();
    }
    return total_size;
  }

 private:
  dense::map<FileID, MacroViewList> storage;
  SegStringSet strings;
  mutable std::shared_mutex mutex;

  std::string_view emplace_string(const std::string& str) {
    return *strings.emplace(str).first;
  }
};
