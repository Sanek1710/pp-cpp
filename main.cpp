#include <cstddef>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

#include "Cursor.h"
#include "helper.h"


// #define debug

int main(int argc, char* argv[]) {
  timeit;
  checkin;
//~8.9 Mb
#ifdef debug
  std::string src = read_file(ROOT "/Cursor.h");
  src += read_file(ROOT "/helper.h");
#else
  std::string src = read_file(ROOT "/sqliteall.c");
#endif
  printit(src.size());
  std::string out;

  if (true) {
    timeit;
    Tokeniser ppm{src};
    ppm.process_code();
    // printit(it.nleft());
  }
  write_file(ROOT "/out.pp.c", out);
#ifdef debug
  return true;
#endif
  size_t summer = 0;

  if (true) {
    //~890 Mb benchmark
    stimeit("process_code 100 times");
    std::string out;
    repeat(10) {
      repeat(10) {
        Tokeniser ppm{src};
        ppm.process_code();
      }
      // untimeit;
      // usleep(200000);
    }
    summer += out.size();
  }

  printit(out.size());
  printit(summer);
  return 0;
}