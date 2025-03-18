#pragma once

#include <vector>

#include "Token.h"

struct MacroInfo {
  unsigned short nargs = 0;
  bool is_variadic = false;
  bool is_functional = false;
};

struct ImageBase {
  Token name;
};

using IncludeImage = ImageBase;
using UndefImage = ImageBase;

struct DefineImage : ImageBase {
  MacroInfo info;
  std::vector<Token> tokens;

  inline void clear() {
    tokens.clear();
    info = MacroInfo{};
  }

  inline auto args_begin() const { return tokens.begin(); }
  inline auto args_end() const { return tokens.begin() + info.nargs; }
  inline auto& args_back() const { return *std::prev(args_end()); }

  inline auto expansion_begin() const { return args_end(); }
  inline auto expansion_end() const { return tokens.end(); }

  void print(std::ostream& os, std::string_view src) const;
};
