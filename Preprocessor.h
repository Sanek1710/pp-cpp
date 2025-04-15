#pragma once

// #define debug
#include <string_view>
#include <vector>

#include "Cursor.h"
#include "Macro.h"
#include "Token.h"
#include "TokenCodeWriter.h"
#include "TokenGroup.h"
#include "util/ranges.h"
#include "util/util.h"

#ifdef debug
#define DBG(...) __VA_ARGS__
#else
#define DBG(...)
#endif

inline Token cat_tokens(Token lhs, Token rhs, std::string& text) {
  text += lhs.get_text();
  text += rhs.get_text();
  // TODONOW: deduce tag from both tags
  Tokeniser tkz{text, lhs.start_pos};

  tkz.read_pptoken();
  constexpr const auto cat_tags = [](Tag lhs, Tag rhs) { return lhs; };

#ifdef ENDPOS
  return make_token(text, cat_tags(lhs.tag, rhs.tag), lhs.start_pos,
                    rhs.end_pos);
#else
  return make_token(text, cat_tags(lhs.tag, rhs.tag), lhs.start_pos);
#endif
}

class Preprocessor {
  using TokenList = std::vector<Token>;
  using TokenListSlice = IndexRange<TokenList>;

  using IndexList = std::vector<size_t>;
  using IndexListSlice = IndexRange<IndexList>;

  using MacroMapValue = std::string;
  using MacroMapValuePtr = MacroMapValue*;

  using StringStorage = dense::segmented_vector<std::string>;

  class TokenBuffWriter {
   public:
    TokenBuffWriter(TokenList& buffer, StringStorage& string_storage)
        : buffer(buffer), head(buffer.size()), string_storage(string_storage) {}

    void write(Token tok) {
      if (cat_mode) return cat_token(tok);
      buffer.push_back(tok);
    }

    TokenListSlice as_input() const { return {buffer, head}; }
    inline void clear() const { buffer.resize(head); }
    inline void set_cat(bool value = true) { cat_mode = true; }

   private:
    TokenList& buffer;
    size_t head;
    StringStorage& string_storage;
    bool cat_mode = false;

    void cat_token(Token tok) {
      cat_mode = false;
      if (buffer.size() <= head) return buffer.push_back(tok);

      std::string& text = string_storage.emplace_back(buffer.back().get_text());
      text += tok.get_text();

      Tokeniser tkz(text, buffer.back().start_pos);
      buffer.pop_back();
      while (!tkz.eof()) buffer.push_back(tkz.read_pptoken());
    }
  };

 public:
  void process_code(std::string_view src_orig, std::string& output);
  void clear();

 private:
  SegStringMap<MacroMapValue> macro_map;
  StringStorage string_storage;
  TokenList out;
  TokenList buf;
  IndexList arg_rages;  // TokenList independent

  std::vector<std::string_view> macro_stack;

  std::vector<positer> offsets;
  std::vector<positer>::iterator offsets_it;

  size_t line_offset = 0;
  positer last_newline_offset = 0;
  std::string src;

  // TODO: try to add macro call view?
  struct MacroUseView {
    TokenListSlice input;
    IndexListSlice arg_chunk;
    MacroStamp macro_stamp;
  };

  // returns amount of valid prescanned macro related tokens
  size_t prescan_macro(TokenListSlice input,  //
                       MacroStamp macroStamp);
  void preprocess_tokens(TokenListSlice input,  //
                         TokenList& buffer,     // same as input on start
                         TokenList& output);

  // TODO: unify:

  size_t prescan_tkz_macro(Token token, Tokeniser& tkz,  //
                           TokenList& output,            //
                           MacroStamp macroStamp);
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
  void prepreprocess();
  Token read_token(Tokeniser& tkz);

  // TODO: move to handler
  void process_ppline(const DirectiveTokenImage& directive);
};
