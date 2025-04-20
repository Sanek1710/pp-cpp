#pragma once

// #define debug
#include <ostream>
#include <string_view>
#include <vector>

#include "CompiledMacro.h"
#include "TokenCodeWriter.h"
#include "tkz/Cursor.h"
#include "tkz/Token.h"
#include "tkz/TokenGroup.h"
#include "util/ranges.h"
#include "util/util.h"

using TokenList = std::vector<Token>;
using TokenListSlice = IndexRange<TokenList>;
using StringStorage = dense::segmented_vector<std::string>;

template <typename DirectiveHandlerImpl>
class DirectiveHandler {
 public:
  void handle(DirectiveTokenImage& directive_image) {
    impl()->handle(directive_image);
  }

 private:
  inline DirectiveHandlerImpl* impl() const {
    return static_cast<DirectiveHandlerImpl*>(this);
  }
};

class DirectiveDumper : public DirectiveHandler<DirectiveDumper> {
 public:
  DirectiveDumper() {}
  void handle(DirectiveTokenImage& directive_image) {}
};

template <typename Iter>
std::ostream& operator<<(std::ostream& os, Range<Iter> range) {
  os << "[";
  for (auto val : range) {
    os << val << ", ";
  }
  return os << "]";
}
template <typename Container>
std::ostream& operator<<(std::ostream& os, IndexRange<Container> range) {
  os << "[";
  for (auto val : range) {
    os << val << ", ";
  }
  return os << "]";
}

class Preprocessor {
  using IndexList = std::vector<size_t>;
  using IndexListSlice = IndexRange<IndexList>;

 public:
  void process_code(std::string& src, std::string& output);
  void collect_macro_map(std::string& src) {
    clear();
    prepreprocess(src);
    Tokeniser tkz{src};
    while (!tkz.eof()) read_token(tkz);
  }
  void clear();

  MacroMap&& take_macro_map() { return std::move(macro_map); }
  const MacroMap& get_macro_map() const { return macro_map; }

  MacroMap& context() { return context_macro_map; }

  void merge_context() {
    for (auto& [name, compiled] : macro_map)
      context_macro_map.emplace(std::move(name), std::move(compiled));
    macro_map.clear();
  }

 private:
  MacroMap macro_map;
  MacroMap context_macro_map;

  // for token cancat result texts
  StringStorage string_storage;

  TokenList out;
  TokenList buf;
  IndexList arg_rages;  // TokenList independent

  std::vector<std::string_view> macro_stack;

  std::vector<positer> offsets;
  std::vector<positer>::iterator offsets_it;

  size_t line_offset = 0;
  positer last_newline_offset = 0;

  // TODO: try to add macro call view?
  struct MacroUseView {
    TokenListSlice input;
    IndexListSlice arg_chunk;
    MacroStamp macro_stamp;
  };

  // returns amount of valid prescanned macro related tokens
  size_t prescan_macro(TokenListSlice input,  //
                       MacroStamp macro_stamp);
  void preprocess_tokens(TokenListSlice input,  //
                         TokenList& buffer,     // same as input on start
                         TokenList& output);

  // TODO: unify:

  size_t prescan_tkz_macro(Token token, Tokeniser& tkz,  //
                           TokenList& output,            //
                           MacroStamp macro_stamp);
  void preprocess_tkz_tokens(Tokeniser& tkz, TokenCodeWriter& writer);

  // macro expansion methods

  void expand_macro(MacroStamp macro_stamp,                          //
                    TokenListSlice input, IndexListSlice arg_chunk,  //
                    TokenList& buffer,                               //
                    TokenList& output);

  // helper for expansion
  TokenListSlice get_arg_range(TokenList& src, const IndexListSlice& arg_chunk,
                               uint16_t arg_idx);

  // macro map logic
  std::optional<MacroStamp> lookup_macro(Token& token) const;
  bool in_process(std::string_view macro_name) const;

  // true on final mark (aka `')'`)
  bool mark_arg_ranges(Tag tag, size_t index, int& balance);

  // token read related methods
  void prepreprocess(std::string& src);
  Token read_token(Tokeniser& tkz);

  // TODO: move to handler
  void process_ppline(const DirectiveTokenImage& directive);

  bool fit_args(MacroInfo info, TokenList& src, size_t args_head);
};
