#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <ostream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "MacroStamp.h"
#include "FileID.h"
#include "Preprocessor.h"
#include "Storage.h"
#include "TokenPrinter.h"
#include "util/bytesize.h"
#include "util/helper.h"
#include "util/progress.h"
#include "util/rwfile.h"
#include "util/timer.h"

std::vector<std::string> walk(std::filesystem::path project_path) {
  const static std::vector<std::string> extensions{
      ".c", ".cc", ".cpp", ".c++", ".cp", ".cxx", ".h", ".hpp"};

  size_t nfiles = 0;

  for (auto const& dir_entry : std::filesystem::recursive_directory_iterator{
           project_path,
           std::filesystem::directory_options::skip_permission_denied})
    ++nfiles;

  auto directory_it = std::filesystem::recursive_directory_iterator{
      project_path, std::filesystem::directory_options::skip_permission_denied};

  std::vector<std::string> filenames;
  size_t total_file_size = 0;
  {
    IncrementalProgrssBar pb;
    for (auto const& dir_entry : directory_it) {
      --nfiles;
      std::error_code ec{};
      if (!dir_entry.is_regular_file(ec)) continue;
      if (ec != std::error_code{}) continue;
      if (std::find(extensions.begin(), extensions.end(),
                    dir_entry.path().extension().generic_string()) ==
          extensions.end())
        continue;
      size_t file_size = dir_entry.file_size(ec);
      if (file_size > (50UL << 20U)) {
        std::cerr << "[skipped]\n";
        continue;
      }
      total_file_size += file_size;
      ++nfiles;
      pb(nfiles);
      filenames.push_back(dir_entry.path().generic_string());
      // if (filenames.size() > 142408 / 16) break;
    }
  }

  std::cerr << "total files: " << filenames.size() << "\n";
  std::cerr << "total size : " << ByteSize{total_file_size} << "\n";

  return filenames;
}

int main(int argc, char* argv[]) {
  std::string project_path = "/home/user/playground/pp-cpp/.tmp/OpenBCM";
  std::vector<std::string> filenames = walk(project_path);

  Storage storage;
  Preprocessor pp;
  std::string src;
  std::string out;
  {
    IncrementalProgrssBar pb;
    timeit;
    FileID file_id = 0;
    for (const auto filename : filenames) {
      {
        untimeit;
        read_file(filename, src);
        notignore += src.size();
      }
      pb(filenames.size());
      // pp.process_code(src, out);
      pp.collect_macro_map(src);
      storage.insert(file_id, pp.get_macro_map());
      ++file_id;
    }
  }
  std::cerr << "macro storage size: " << storage.memory() << "\n";

  {
    IncrementalProgrssBar pb;
    timeit;
    FileID file_id = 0;
    for (const auto filename : filenames) {
      pb(filenames.size());
      storage.get(file_id, pp.context());
      ++file_id;
    }
  }
  std::cerr << "total unique macros: " << pp.context().size() << "\n";

  {
    IncrementalProgrssBar pb;
    timeit;
    FileID file_id = 0;
    for (const auto filename : filenames) {
      {
        untimeit;
        read_file(filename, src);
      }
      pb(filenames.size());
      pp.process_code(src, out);
      ++file_id;
    }
  }

  return 0;
}