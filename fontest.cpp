#include <cctype>
#include <iostream>
#include <ostream>
#include "helper.h"

int main(int argc, char* argv[]) {
  std::cerr << ctrl_str{"Hello\nworld\rhow\tare\vyou"} << "\n";
  std::cerr << "\n";
  return 0;
  //Helloêworldíhowéareëyou;
  
}
