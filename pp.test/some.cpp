
#include <iostream>

#define Some const char* str = /*







*/ R"(\
define Thing )"


int main(int argc, char *argv[]) {
  Some;
  std::cerr << str << std::endl;
  return 0;
}