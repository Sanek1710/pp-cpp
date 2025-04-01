

#include <cstddef>
#include <ostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "memstat.h"
#include "util/util.h"

using FileID = unsigned;
using MacroName = std::string;
using Version = unsigned;  // 2^32 seconds ~130 years

// Simple macro expansion for testing
struct MacroStamp {
  std::string content;
};

struct Macros {
  MacroName name;
  MacroStamp expansion;
};

struct MacroDefinition {
  FileID fileId;
  Version version;
  MacroStamp expansion;
};
template <>
inline size_t memory(const MacroDefinition& def) {
  size_t nbytes = 0;
  nbytes += sizeof(MacroDefinition);
  nbytes += memory(def.expansion.content);
  return nbytes;
}

template <typename Key, typename Value>
inline size_t memory(const dense::map<Key, Value>& mp) {
  size_t nbytes = mp.bucket_count() * (sizeof(size_t) + sizeof(size_t));
  for (const auto& p : mp) {
    nbytes += memory(p);
  }
  return nbytes;
}

class MacroStorage {
  static constexpr Version originalVersion = 0;

 public:
  using FileMacrosList = std::vector<std::pair<MacroName, MacroStamp>>;

  void updateFileMacros(FileID fileId, const FileMacrosList& newMacros) {
    Version currentVersion = updateFileVersion(fileId);
    for (const auto& [name, expansion] : newMacros) {
      auto& defs = macros[name];
      // optionally, might be smarted where we replace them on same places
      cleanupOldMacroses(defs, fileId, currentVersion);
      defs.push_back({fileId, currentVersion, expansion});
    }
  }

  void insertFileMacros(FileID fileId, const FileMacrosList& newMacros) {
    Version currentVersion = getFileVersion(fileId);
    for (const auto& [name, expansion] : newMacros) {
      auto& defs = macros[name];
      defs.push_back({fileId, currentVersion, expansion});
    }
  }

  void deleteFileMacros(FileID fileId) { updateFileVersion(fileId); }

  const MacroStamp* findMacro(const MacroName& name,
                              const dense::set<FileID>& includeIds) const {
    auto macroIt = macros.find(name);
    if (macroIt == macros.end()) return nullptr;

    // Check each definition
    for (const auto& def : macroIt->second) {
      if (!includeIds.count(def.fileId)) continue;

      // optionally cleanup?
      // warning doubly looked up for each macros
      // but shouldn't take too long
      // stats: 1M lookups
      //      version: 2.62515s
      //   no version: 2.47522s
      Version version = getFileVersion(def.fileId);

      if (def.version < version) continue;
      return &def.expansion;
    }

    return nullptr;
  }

  void memstats(std::ostream& os) {
    os << "MacroStorage memstats:\n";
    os << " - macros      : " << memory(macros) / 1024. / 1024. << "Mb\n";
    os << " - fileVersions: " << memory(fileVersions) / 1024. / 1024. << "Mb\n";
    os << "```````````````````````````````````````````\n";
  }

  // consider deepclean method

 private:
  // optionally split onto Functional and Object macros
  dense::map<MacroName, std::vector<MacroDefinition>> macros;
  // only updated/deleted files
  dense::map<FileID, Version> fileVersions;

  Version getFileVersion(FileID fileId) const {
    if (auto fileVersIt = fileVersions.find(fileId);
        fileVersIt != fileVersions.end())
      return fileVersIt->second;
    return originalVersion;
  }

  Version updateFileVersion(FileID fileId) { return ++fileVersions[fileId]; }

  void cleanupOldMacroses(std::vector<MacroDefinition>& defs, FileID fileId,
                          Version currentVersion) {
    defs.erase(
        std::remove_if(defs.begin(), defs.end(),
                       [fileId, currentVersion](const MacroDefinition& def) {
                         return def.fileId == fileId &&
                                def.version <= currentVersion;
                       }),
        defs.end());
  }
};