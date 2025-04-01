#pragma once

#include <type_traits>
#include <vector>

#include "Token.h"
#include "util/RangeView.h"

using TokenList = std::vector<Token>;

struct MacroInfo {
  unsigned short nargs = 0;
  bool is_variadic = false;
  bool is_functional = false;
};

struct IncludeTokenImage;
struct DefineTokenImage;
struct UndefTokenImage;

struct DirectiveTokenImage {
  void print(std::ostream& os) const;

  inline void clear() {
    tokens.clear();
    kind = Undefined;
    details = {0};
  }

  template <typename TokenImageType>
  const TokenImageType& as() const {
    static_assert(std::is_base_of_v<DirectiveTokenImage, TokenImageType>);
    static_assert(sizeof(DirectiveTokenImage) == sizeof(TokenImageType));

    return static_cast<const TokenImageType&>(*this);
  }

 protected:
  enum Kind { Include, Define, Undef, Undefined };
  union Details {
    MacroInfo macroInfo;
  };

  Token base_token;
  Kind kind;
  Details details = {0};
  TokenList tokens;

  friend class Tokeniser;
};

struct IncludeTokenImage : public DirectiveTokenImage {
  Token include_str() const { return base_token; }
};
struct UndefTokenImage : public DirectiveTokenImage {
  Token name() const { return base_token; }
};
struct DefineTokenImage : public DirectiveTokenImage {
  Token name() const { return base_token; }
  MacroInfo info() const { return details.macroInfo; }
  inline auto args_view() const {
    return Range{tokens.begin(), tokens.begin() + info().nargs};
  }
  inline auto expansion_view() const {
    return Range{tokens.begin() + info().nargs, tokens.end()};
  }

  void print(std::ostream& os) const;
};
