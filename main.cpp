#include <unistd.h>

#include "ExpansionTokeniser.h"
#include "Preprocessor.h"
#include "TokenPrinter.h"
#include "tkz/Cursor.h"
#include "tkz/TokenGroup.h"
#include "util/helper.h"
#include "util/indentos.h"
#include "util/rwfile.h"
#include "util/testit.h"
#include "util/timer.h"

void print_source(std::string_view src) {
  Tokeniser tkz{src};
  TokenPrinter printer{std::cerr, true};
  while (!tkz.eof()) printer.print(tkz.read_token(), tkz.cursor().peek());
  std::cerr << "\n";
}

int perf_test(const std::string& filename) {
  std::string src = read_file(filename);
  std::string out;
  timeit;
  repeat(5) {
    repeat(20) {
      Preprocessor pre;
      pre.process_code(src, out);
    }
    untimeit;
    usleep(500'000);
  }
  return 0;
}

int out_test(const std::string& filename, const std::string& outfilename,
             const std::string& expfilename) {
  std::string src = read_file(filename);
  timeit;
  Preprocessor pre;
  std::string out;
  pre.process_code(src, out);

  if (!outfilename.empty()) {
    write_file(outfilename, out);
  } else if (expfilename.empty()) {
    print_source(out);
  }

  if (!expfilename.empty()) {
    std::string exp = read_file(expfilename);
    std::cerr << "\033[34m" << filename << " <> " << expfilename << "\033[0m\n";

    if (out == exp) {
      std::cerr << "  \033[32m[pass]\033[0m\n";
    } else {
      std::cerr << "  \033[31m[fail]\033[0m\n";
    }
  }

  return 0;
}

int main(int argc, char* argv[]) {
  std::string filename;
  std::string outfilename;
  std::string expfilename;

  bool perf = false;
  bool out = false;
  bool exp = false;

  for (int i = 1; i < argc; ++i) {
    std::string arg{argv[i]};

    if (out) {
      outfilename = arg;
      out = false;
      continue;
    }
    if (exp) {
      expfilename = arg;
      exp = false;
      continue;
    }

    if (!arg.empty() && arg.front() == '-') {
      if (arg == "--perf")
        perf = true;
      else if (arg == "-o") {
        out = true;
      } else if (arg == "-e") {
        exp = true;
      }
    } else {
      if (!filename.empty()) {
        std::cerr << "invalid args: more than one filename\n";
        return -1;
      }
      filename = arg;
    }
  }

  if (filename.empty()) {
    std::cerr << "invalid args: no filename\n";
    return -1;
  }

  if (perf) return perf_test(filename);
  out_test(filename, outfilename, expfilename);

  return 0;
}
