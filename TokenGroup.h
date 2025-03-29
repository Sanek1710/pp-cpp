#pragma once

#include <vector>

#include "Token.h"
#include "RangeView.h"

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

  inline auto args_view() const {
    return Range{tokens.begin(), tokens.begin() + info.nargs};
  }
  inline auto expansion_view() const {
    return Range{tokens.begin() + info.nargs, tokens.end()};
  }

  void print(std::ostream& os) const;
};
