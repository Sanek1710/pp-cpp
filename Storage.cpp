#include "Storage.h"

#include <iostream>

int main(int argc, char* argv[]) {
  Storage storage;

  MacroMap macro_map0{
      {"TWO_WORDS", "f0 aa bb"},                 //
      {"STR_AFTER_TOKEN", "f1 aa $0s"},          //
      {"NAMED_VA", "v1 some($0a)"},              //
      {"UNNAMED_VA", "v1 some($0a)"},            //
      {"INCNAMED_VA", "v1 some(__VA_ARGS__)"},   //
      {"INCUNNAMED_VA", "v1 some(named)"},       //
      {"CNSTINBOOL", " constexpr inline bool"},  //
      {"farg", "f1 farg($0a)"},                  //
      {"f", "f0 f()"},                           //
      {"abeta", "f1 alpha$0rgamma"},             //
      {"SELF1", "f2 ($0a, $1a)"},                //
  };
  MacroMap macro_map1{
      {"farg", "f1 farg($0a)"},       //
      {"f", "f0 f()"},                //
      {"abeta", "f1 alpha$0rgamma"},  //
      {"SELF1", "f2 ($0a, $1a)"},     //
      {"CAT1", "f2 $0r$1r"},          //
      {"STR1", "f1 $0s"},             //
      {"STR1", "f1 $0s"},             //
      {"CATSTR1", "f2 $0a$1s"},       //
      {"CATSTR2", "f2 $0r$1s"},       //
      {"CATSTR3", "f2 $0r$1s"},       //
      {"CATSTR4", "f2 $0s$1s"},       //
      {"CATSTR5", "f2 $0r$1r"},       //
  };

  storage.insert(0, macro_map0);
  storage.insert(1, macro_map1);
  storage.print(std::cerr);
  std::cerr << "\n";

  MacroMap got_it;
  storage.get(0, got_it);
  storage.get(1, got_it);
  std::cerr << got_it;

  return 0;
}