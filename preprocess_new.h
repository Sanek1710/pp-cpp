#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "ExpansionTokeniser.h"
#include "Macro.h"
#include "Token.h"
#include "TokenGroup.h"
#include "util/VectorTail.h"
#include "util/util.h"

template <typename TokeniserT>
class TokeniserIterator {
  TokeniserIterator(TokeniserT& tkz) : tkz(tkz) { token = tkz.read_token(); }

  TokeniserIterator& operator++() {
    token = tkz.read_token();
    return *this;
  }
  Token& operator*() { return token; }
  Token* operator->() { return &token; }
  bool eof() const { return tkz.eof(); }

 private:
  TokeniserT& tkz;
  Token token;
};

class Preprocessor {
  using IndexList = std::vector<size_t>;
  using IndexListTail = VectorTail<size_t>;
  using TokenListTail = VectorTail<Token>;

  using MacroMapValue = std::string;
  using MacroMapValuePtr = MacroMapValue*;
  SegStringMap<MacroMapValue> macroMap;

  TokenList out;
  TokenList buffer;
  IndexList arg_rages;  // TokenList independent

  std::vector<std::string_view> macro_stack;

  DirectiveTokenImage tokenImage;

  inline auto arg_range(TokenListTail& exp_chunk,  //
                        IndexListTail& arg_chunk, size_t arg_idx) {
    return Range{exp_chunk.begin() + arg_chunk[arg_idx] + 1,
                 exp_chunk.begin() + arg_chunk[arg_idx + 1]};
  }

  inline auto arg_index_range(VectorTail<Token>& exp_chunk,
                              VectorTail<size_t>& arg_chunk, size_t arg_idx) {
    return IndexRange{ 
        exp_chunk.base(),
        exp_chunk.head() + arg_chunk[arg_idx] + 1,
        exp_chunk.head() + arg_chunk[arg_idx + 1],
    };
  }

  class TokeniserInterface {
   public:
    TokeniserInterface(std::vector<Token>& tokens, size_t begin_idx,
                       size_t end_idx)
        : tokens(tokens), idx(begin_idx), end_idx(end_idx) {}
    inline Token read_token() {
      if (idx == end_idx) return Token{.tag = tag::eof};
      return tokens[idx++];
    }

   private:
    std::vector<Token>& tokens;
    size_t idx;
    const size_t end_idx;
  };

  std::optional<MacroStamp> lookup_macro(Token& token) {
    if (token.details.marker) return std::nullopt;
    const auto macro_name = token.get_text();
    auto macroIt = macroMap.find(macro_name);
    if (macroIt == macroMap.end()) {
      token.details.marker = true;
      return std::nullopt;
    }
    token.details.marker = false;
    if (in_process(macro_name)) return std::nullopt;
    return MacroStamp{macroIt->second};
  }

  // returns amount of valid prescanned tokens
  size_t prescan_macro(TokenListTail input, size_t size,  //
                       MacroStamp macroStamp, IndexListTail arg_chunk) {
    if (!macroStamp.info.is_functional) return 1;
    // token 0 is macroname
    int i = 0;
    // skip extras
    do {
      if (++i >= size) return 0;
    } while (tag::is_extra(input[i].tag));
    // next after extras should be '('
    // as it supposed to be macro call
    if (input[i].tag != tag::raw('(')) return 0;
    // push args start
    arg_chunk.push_back(i);

    int balance = 1;
    for (++i; i < size; ++i) {
      switch (input[i].tag) {
        case tag::raw('('): {
          ++balance;
          break;
        }
        case tag::raw(','): {
          if (balance == 1) arg_chunk.push_back(i);
          break;
        }
        case tag::raw(')'): {
          --balance;
          if (balance != 0) break;
          arg_chunk.push_back(i);
          const size_t nargs = arg_chunk.size() - 1;
          if (macroStamp.is_valid_call(nargs)) return ++i;
          // invalid macro use: nargs mismatch
          arg_chunk.clear();
          return 0;
        }
        default:
          break;
      }
    }
    // invalid macro use: eof
    arg_chunk.clear();
    return 0;
  }

  void expand_macro(MacroStamp macro_stamp, TokenListTail source,
                    IndexListTail arg_chunk, TokenListTail output) {
    TokenListTail preexpanion{source.slice()};
    const Token macro_token = source.front();
    macro_stack.push_back(macro_token.get_text());

    MacroExpansionTokeniser macro_tkz{macro_stamp.expansion};
    while (!macro_tkz.eof()) {
      Token token = macro_tkz.read_token();
      if (token.tag == tag::arg) {
        const size_t arg_idx = token.details.index;
        const size_t arg_ntokens =
            arg_chunk[arg_idx + 1] - arg_chunk[arg_idx] + 1;
        if (true /*not concat or stringify*/) {
          preprocess_tokens(source.slice(arg_chunk[arg_idx + 1]), arg_ntokens,
                            preexpanion);
        } else if (false /*stringify*/) {
          // stringify all tokens
        } else if (false /*concat*/) {
          // copy all tokens as are
        }
        continue;
      }
      token.start_pos.line = macro_token.start_pos.line;
      token.start_pos.column += macro_token.start_pos.column;
      token.end_pos.line += macro_token.start_pos.line;
      token.end_pos.column += macro_token.start_pos.column;
      preexpanion.push_back(token);
    }
    // clear applied args
    arg_chunk.clear();
    preprocess_tokens(preexpanion, preexpanion.size(), output);
    preexpanion.clear();
    macro_stack.pop_back();
  }

  void preprocess_tokens(TokenListTail input, size_t size,
                         TokenListTail output) {
    // size = std::min(size, input.size());
    while (size) {
      switch (input.front().tag) {
        case tag::identifier: {
          auto oMacroStamp = lookup_macro(input.front());
          if (!oMacroStamp) break;

          IndexListTail arg_chunk{arg_rages};
          const size_t nprescanned =
              prescan_macro(input, size, *oMacroStamp, arg_chunk);
          if (nprescanned == 0) break;
          expand_macro(*oMacroStamp, input, arg_chunk, output);
          input.remove_prefix(nprescanned);
          size -= nprescanned;
          continue;
        }
        default:
          break;
      }
      output.push_back(input.front());
      input.remove_prefix(1);
      size -= 1;
    }
  }

 public:
  template <typename TokeniserT>
  Token process_macro_call(Token macro_token, TokeniserT& tkz,
                           MacroStamp macroStamp) {
    VectorTail expansion_chunk{out};
    VectorTail arg_chunk{arg_rages};

    if (!macroStamp.info.is_functional) {
      expand_macro(macro_token, macroStamp, expansion_chunk, arg_chunk);
      return tkz.read_token();
    }
    // here we skipping all extras, without writing them anywhere
    // if one need them for some reason, one should implement smarter dump logic
    // good luck
    expansion_chunk.push_back(macro_token);

    Token token = tkz.read_token();
    while (tag::is_extra(token.tag)) {
      expansion_chunk.push_back(token);
      token = tkz.read_token();
    }

    if (token.tag != tag::raw('(')) return token;

    arg_chunk.push_back(expansion_chunk.size());

    static constexpr unsigned balance_origin = 0;
    int balance = balance_origin;

    while (token.tag != tag::eof) {
      // TODONOW: preserve at least one space
      if (tag::is_extra(token.tag)) {
        expansion_chunk.push_back(code_token(" ", token.start_pos));
        do {
          token = tkz.read_token();
        } while (tag::is_extra(token.tag));
        continue;
      }
      switch (token.tag) {
        case tag::raw('('): {
          ++balance;
          break;
        }
        case tag::raw(')'): {
          --balance;
          if (balance != balance_origin) break;
          // remember end position for last arg
          arg_chunk.push_back(expansion_chunk.size());
          expansion_chunk.push_back(token);
          // god forgive me
          goto process_expansion;
        }

        case tag::raw(','): {
          if (balance == balance_origin + 1)
            arg_chunk.push_back(expansion_chunk.size());
          break;
        }

        case tag::identifier: {
          const auto macro_name = token.get_text();
          auto macroIt = macroMap.find(macro_name);
          if (macroIt == macroMap.end()) {
            token.details.marker = true;
            break;
          }
          token.details.marker = false;
          if (in_process(macro_name)) break;

          token = process_macro_call(token, tkz, MacroStamp{macroIt->second});
          continue;
        }

        default:
          break;
      }
      expansion_chunk.push_back(token);
      token = tkz.read_token();
    }

  process_expansion:

    const size_t nargs_input = arg_chunk.size() - 1;
    if (token.tag != tag::eof && macroStamp.is_valid_call(nargs_input)) {
      // to this point we have whole bunch of expansion relevant tokens
      // also marked argument ranges
      // also argument count
      // what do we do with it?

      // we expand it into buffer

      expand_macro(macro_token, macroStamp, expansion_chunk, arg_chunk);
    } else {
      arg_chunk.clear();
    }
    return tkz.read_token();
  }
  void expand_macro(Token macro_token, MacroStamp macroStamp,
                    VectorTail<Token> exp_chunk, VectorTail<size_t> arg_chunk) {
    VectorTail buffer_chunk{buffer};
    // TODO: support base position for tokenisers
    MacroExpansionTokeniser macro_tkz{macroStamp.expansion};
    while (!macro_tkz.eof()) {
      Token token = macro_tkz.read_token();
      if (token.tag == tag::arg) {
        const size_t arg_idx = token.details.index;
        for (const auto& arg_tok :
             arg_index_range(exp_chunk, arg_chunk, arg_idx)) {
          buffer_chunk.push_back(arg_tok);
        }

        continue;
      }
      token.start_pos.line = macro_token.start_pos.line;
      token.start_pos.column += macro_token.start_pos.column;
      token.end_pos.line += macro_token.start_pos.line;
      token.end_pos.column += macro_token.start_pos.column;
      buffer_chunk.push_back(token);
    }

    exp_chunk.clear();
    arg_chunk.clear();

    // TODO: treat bufer as new input to read from
    // then expand back to output on place of previous tokens
    // out.insert(out.end(), buffer.begin() + buffer_head, buffer.end());
    macro_stack.push_back(macro_token.get_text());
    post_process_expansion(buffer_chunk);
    macro_stack.pop_back();

    buffer_chunk.clear();
  }

  const auto& process_code(std::string_view src) {
    out.clear();
    buffer.clear();
    arg_rages.clear();
    macro_stack.clear();

    Tokeniser tkz = Tokeniser{src, tokenImage};
    Token token = tkz.read_token();
    while (token.tag != tag::eof) {
      switch (token.tag) {
        case tag::pp_include: {
          const IncludeTokenImage& includeImage =
              tokenImage.as<IncludeTokenImage>();
          // totaltimeit;
          break;
        }

        case tag::pp_define: {
          const DefineTokenImage& defineImage =
              tokenImage.as<DefineTokenImage>();
          macroMap.emplace(defineImage.name().get_text(),
                           compile_macro_expansion(defineImage));
          break;
        }

        case tag::pp_undef: {
          const UndefTokenImage& undefImage = tokenImage.as<UndefTokenImage>();
          macroMap.erase(undefImage.name().get_text());
          break;
        }

        case tag::identifier: {
          const auto macro_name = token.get_text();
          auto macroIt = macroMap.find(macro_name);
          if (macroIt == macroMap.end()) {
            token.details.marker = true;
            break;
          }
          token.details.marker = false;
          if (in_process(macro_name)) break;

          token = process_macro_call(token, tkz, MacroStamp{macroIt->second});
          // out.clear();
          continue;
        }
        default:
          break;  // from switch
      }
      // out.push_back(token);
      token = tkz.read_token();
    };

    once {
      notignore += out.size();
      printit(out.size());
    };
    return out;

  }  // const auto& process_code(std::string_view src) {

  inline void post_process_expansion(VectorTail<Token> buffer_chunk) {
    TokeniserInterface tkz{buffer, buffer_chunk.head(),
                           buffer_chunk.head() + buffer_chunk.size()};
    Token token = tkz.read_token();
    while (token.tag != tag::eof) {
      switch (token.tag) {
        case tag::identifier: {
          const auto macro_name = token.get_text();
          auto macroIt = macroMap.find(macro_name);
          if (macroIt == macroMap.end()) {
            token.details.marker = true;
            break;  // from switch
          }
          token.details.marker = false;
          if (in_process(macro_name)) break;

          token = process_macro_call(token, tkz, MacroStamp{macroIt->second});
          continue;
        }
        default:
          break;
      }
      out.push_back(token);
      token = tkz.read_token();
    }
  }

 private:
  inline bool in_process(std::string_view macro_name) {
    return std::find(macro_stack.rbegin(), macro_stack.rend(), macro_name) !=
           macro_stack.rend();
  }
};
