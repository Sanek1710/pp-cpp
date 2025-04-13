#pragma once

#include <type_traits>
#include <vector>

#include "Token.h"
#include "util/RangeView.h"

struct MacroInfo {
  unsigned short nargs = 0;
  bool is_variadic = false;
  bool is_functional = false;
};

struct IncludeInfo {
  bool is_system = false;
};

struct IncludeView;
struct DefineView;
struct UndefView;

struct DirectiveTokenImage {
  enum Kind { Other, Include, Define, Undef, Invalid };

  void print(std::ostream& os) const;

  inline void clear() {
    tokens.clear();
    mkind = Other;
    details = {0};
  }

  Token directive() const { return directive_token; }
  const IncludeView& as_include() const { return as<IncludeView>(); }
  const DefineView& as_define() const { return as<DefineView>(); }
  const UndefView& as_undef() const { return as<UndefView>(); }
  Kind kind() const { return mkind; }

 protected:
  union Details {
    MacroInfo macroInfo;
    IncludeInfo includeInfo;
  };

  Token directive_token;
  Token base_token;
  Kind mkind;
  Details details = {0};
  std::vector<Token> tokens;

 private:
  template <typename TokenImageType>
  const TokenImageType& as() const {
    static_assert(std::is_base_of_v<DirectiveTokenImage, TokenImageType>);
    static_assert(sizeof(DirectiveTokenImage) == sizeof(TokenImageType));
    return static_cast<const TokenImageType&>(*this);
  }

  friend class Tokeniser;
};

struct IncludeView : public DirectiveTokenImage {
  Token include_str() const { return base_token; }
  IncludeInfo info() const { return details.includeInfo; }
};

struct UndefView : public DirectiveTokenImage {
  Token name() const { return base_token; }
};

struct DefineView : public DirectiveTokenImage {
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
