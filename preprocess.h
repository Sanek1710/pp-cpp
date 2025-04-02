#pragma once

#include <string>
#include <vector>

#include "ExpansionTokeniser.h"
#include "Macro.h"
#include "TokenGroup.h"
#include "util/VectorTail.h"
#include "util/util.h"

class Preprocessor {
  template <typename T>
  static inline void reset(std::vector<T>& container, size_t head) {
    container.erase(container.begin() + head, container.end());
  }
  StringMap<std::string> macroMap;

  std::vector<Token> out;
  std::vector<Token> buffer;
  std::vector<size_t> arg_rages;

  std::string str_buffer;
  std::vector<std::string_view> macro_stack;

  DirectiveTokenImage tokenImage;

  inline auto arg_range(VectorTail<Token>& exp_chunk,
                        VectorTail<size_t>& arg_chunk, size_t arg_idx) {
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
        for (const auto& arg_tok : arg_index_range(exp_chunk, arg_chunk, arg_idx)) {
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
    return std::find(macro_stack.begin(), macro_stack.end(), macro_name) !=
           macro_stack.end();
  }
};
