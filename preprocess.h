#pragma once

#include <algorithm>
#include <cstddef>
#include <iomanip>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "Cursor.h"
#include "ExpansionTokeniser.h"
#include "Macro.h"
#include "Token.h"
#include "TokenGroup.h"
#include "TokenPrinter.h"
#include "ankerl/unordered_dense.h"
#include "helper.h"
#include "util/RangeView.h"
#include "util/util.h"

template <typename TokeniserT>
class TokeniserReader {
 public:
  TokeniserReader(TokeniserT& tkz) : tkz(&tkz) { read(); }

  Token& get() { return token; }
  Token& read() { return token = tkz->read_token(); }
  bool eof() const { return token.tag == tag::eof; }

 private:
  TokeniserT* tkz;
  Token token;
};

class TokenWriter {
 public:
  TokenWriter(std::string& output) : output(output) {}
  void write(Token tok) {
    ++ntokens;
    output += tok.get_text();
    // output.push_back(tok);
  }
  size_t size() const { return ntokens; }

 private:
  std::string& output;
  size_t ntokens = 0;
};

inline std::ostream& operator<<(std::ostream& os,
                                const IndexRange<std::vector<Token>>& tokens) {
  TokenPrinter printer{os, false};
  for (const auto& token : tokens) {
    if (token.tag == tag::newline) {
      os << " \\n ";
    } else {
      printer.print(token);
    }
  }
  os << "\n";
  return os;
}

static constexpr bool oneof(char c, std::string_view char_set) {
  return char_set.rfind(c) != std::string_view::npos;
};

inline constexpr bool incompatible(Tag lhs, Tag rhs) {
  const char mrhs = tag::markerof(rhs);
  // anything after categorised chars
  if (!tag::is_raw(lhs)) {
    // in most cases these are iconpatible
    if (!tag::is_raw(rhs)) return true;
    return lhs == tag::number && oneof(mrhs, ".+-");
  }

  // categorised chars after raw chars
  if (!tag::is_raw(rhs)) return lhs == tag::raw('.') && rhs == tag::number;

  // raw chars after raw chars
  const char mlhs = tag::markerof(lhs);
  switch (mrhs) {
      // clang-format off
      case '%': return oneof(mlhs, ".<");
      case '&': return mlhs == '&';
      case '*': return mlhs == '/';
      case '+': [[fallthrough]];
      case '-': [[fallthrough]];
      case '.': [[fallthrough]];
      case '/': return mlhs == mrhs;
      case ':': return oneof(mlhs, ":%<");
      case '<': return mlhs == '<';
      case '=': return oneof(mlhs, "!%&*+-/<=>|^");
      case '>': return oneof(mlhs, ":%->");
      case '|': return mlhs == '|';
    // clang-format on
    default:
      break;
  }
  // rest are compatible-ish
  return false;
}

untestit(compl ) {
  static_assert(incompatible(tag::identifier, tag::identifier));

  const Tag tags[]{
      tag::raw('!'), tag::raw('.'),   tag::raw(':'),
      tag::raw('%'), tag::raw('&'),   tag::raw('*'),
      tag::raw('+'), tag::raw('-'),   tag::raw('/'),
      tag::raw('<'), tag::raw('='),   tag::raw('>'),
      tag::raw('|'), tag::raw('^'),   tag::raw('`'),
      tag::raw('('), tag::raw(')'),   tag::raw(','),
      tag::raw(';'), tag::raw('?'),   tag::raw('@'),
      tag::raw('['), tag::raw(']'),   tag::raw('{'),
      tag::raw('}'), tag::raw('~'),   tag::string_like_literal,
      tag::number,   tag::identifier,
  };
  for (auto l : tags) {
    for (auto r : tags) {
      std::cerr << (incompatible(l, r) ? '+' : ' ') << "|";
    }
    std::cerr << "\n";
  }
}

class Preprocessor {
  using IndexList = std::vector<size_t>;

  using TokenListSlice = IndexRange<TokenList>;
  using IndexListSlice = IndexRange<IndexList>;

  using MacroMapValue = std::string;
  using MacroMapValuePtr = MacroMapValue*;

  using StringStorage = dense::segmented_vector<std::string>;

 public:
  std::string output;
  const auto& process_code(std::string_view src) {
    Tokeniser tkz{src, tokenImage};

    output.clear();
    TokenWriter writer{output};
    preprocess_tkz_tokens(tkz, writer);

    return output;
  }

  const auto& process_code(std::string_view src, std::string& output) {
    Tokeniser tkz{src, tokenImage};

    output.clear();
    TokenWriter writer{output};
    preprocess_tkz_tokens(tkz, writer);

    return output;
  }

 private:
  SegStringMap<MacroMapValue> macro_map;
  StringStorage string_storage;

  TokenList out;
  TokenList buf;
  IndexList arg_rages;  // TokenList independent

  std::vector<std::string_view> macro_stack;

  DirectiveTokenImage tokenImage;

  // TODO:
  struct MacroUseView {
    TokenListSlice input;
    IndexListSlice arg_chunk;
    MacroStamp macro_stamp;
  };


  // returns amount of valid prescanned tokens
  size_t prescan_macro(TokenListSlice input,  //
                       MacroStamp macroStamp) {
    if (!macroStamp.info.is_functional) return 1;
    // token 0 is macroname
    auto it = input.begin();
    const auto end = input.end();
    // skip extras
    do {
      if (++it >= end) return 0;
    } while (tag::is_extra(it->tag));
    // next after extras should be '('
    // as it supposed to be macro call
    if (it->tag != tag::raw('(')) return 0;
    // push args start
    const size_t args_head = arg_rages.size();
    arg_rages.push_back(it.index());

    int balance = 1;
    for (++it; it < end; ++it) {
      if (!mark_arg_ranges(it->tag, it.index(), balance)) continue;
      const size_t nargs = arg_rages.size() - args_head - 1;
      if (!macroStamp.is_valid_call(nargs)) break;
      return it - input.begin();
    }
    // invalid macro use: eof
    arg_rages.resize(args_head);
    return 0;
  }

  void preprocess_tokens(TokenListSlice input,  //
                         TokenList& buffer,     // same as input on start
                         TokenList& output) {
                          
    while (!input.empty()) {
      switch (input.front().tag) {
        case tag::identifier: {
          const auto oMacroStamp = lookup_macro(input.front());
          if (!oMacroStamp) break;

          const size_t args_head = arg_rages.size();
          const size_t nprescanned = prescan_macro(input, *oMacroStamp);
          if (nprescanned == 0) break;
          expand_macro(*oMacroStamp, input,
                       IndexListSlice{arg_rages, args_head},  //
                       buffer, output);
          input.remove_prefix(nprescanned);
          continue;
        }
        default:
          break;
      }
      output.push_back(input.front());
      input.remove_prefix(1);
    }
  }

  // TODO: unify:

  size_t prescan_tkz_macro(Token token, Tokeniser& tkz,
                           TokenList& output,  //
                           MacroStamp macroStamp) {
    output.clear();
    output.push_back(token);
    if (!macroStamp.info.is_functional) return output.size();

    // skip extras
    do {
      if (tkz.eof()) return 0;
      token = tkz.read_token();
      if (tag::is_ppline(token.tag)) {
        process_ppline(token.tag);
        continue;
      }
      output.push_back(token);
      if (!tag::is_extra(token.tag)) break;
    } while (true);
    // next after extras should be '('
    // as it supposed to be macro call
    if (token.tag != tag::raw('(')) return 0;
    // push args start
    const size_t args_head = arg_rages.size();
    arg_rages.push_back(output.size() - 1);

    int balance = 1;
    while (!tkz.eof()) {
      token = tkz.read_token();
      if (tag::is_ppline(token.tag)) {
        process_ppline(token.tag);
        continue;
      }
      output.push_back(token);
      if (!mark_arg_ranges(token.tag, output.size() - 1, balance)) continue;
      const size_t nargs = arg_rages.size() - args_head - 1;
      if (!macroStamp.is_valid_call(nargs)) break;
      return output.size();
    }
    // invalid macro use
    arg_rages.resize(args_head);
    return 0;
  }

  void preprocess_tkz_tokens(Tokeniser& tkz, TokenWriter& writer) {
    out.clear();
    buf.clear();
    arg_rages.clear();
    macro_stack.clear();
    string_storage.clear();

    while (!tkz.eof()) {
      Token token = tkz.read_token();
      if (tag::is_ppline(token.tag)) {
        process_ppline(token.tag);
        continue;
      }
      switch (token.tag) {
        case tag::identifier: {
          const auto oMacroStamp = lookup_macro(token);
          if (!oMacroStamp) break;

          const size_t args_head = arg_rages.size();
          const size_t nprescanned =
              prescan_tkz_macro(token, tkz, buf, *oMacroStamp);
          IndexListSlice arg_chunk{arg_rages, args_head};

          TokenListSlice buff_input{buf};
          if (nprescanned == 0) {
            writer.write(buf.front());
            buff_input.remove_prefix(1);
            preprocess_tokens(buff_input, buf, out);
          } else {
            expand_macro(*oMacroStamp, buff_input, arg_chunk, buf, out);
          }
          notignore += buf.size();
          notignore += out.size();
          buf.clear();
          for (const auto& token : out) writer.write(token);
          out.clear();
          continue;
        }
        default:
          break;
      }
      writer.write(token);
    }
  }

  inline void push_process_macro(std::string_view macro_name) {
    macro_stack.push_back(macro_name);
  }
  inline void pop_process_macro(std::string_view macro_name) {
    (void)macro_name;
    macro_stack.pop_back();
  }

  inline bool in_process(std::string_view macro_name) const {
    return std::find(macro_stack.rbegin(), macro_stack.rend(), macro_name) !=
           macro_stack.rend();
  }

  std::optional<MacroStamp> lookup_macro(Token& token) const {
    if (token.details.marker) return std::nullopt;
    const auto macro_name = token.get_text();
    auto macroIt = macro_map.find(macro_name);
    if (macroIt == macro_map.end()) {
      token.details.marker = true;
      return std::nullopt;
    }
    token.details.marker = false;
    if (in_process(macro_name)) return std::nullopt;
    return MacroStamp{macroIt->second};
  }



  void expand_macro(MacroStamp macro_stamp,                          //
                    TokenListSlice input, IndexListSlice arg_chunk,  //
                    TokenList& buffer,                               //
                    TokenList& output) {
    size_t buffer_head = buffer.size();
    const Token macro_token = input.front();

    MacroExpansionTokeniser macro_tkz{macro_stamp.expansion};
    Tag last_tag = tag::eof;
    bool need_check = false;

    while (!macro_tkz.eof()) {
      Token token = macro_tkz.read_token();
      // check catenation

      if (tag::is_macro_arg(token.tag)) {
        const size_t arg_ibegin = arg_chunk[token.details.index] + 1;
        const size_t arg_iend = arg_chunk[token.details.index + 1];
        IndexRange arg_tokens{input.base(), arg_ibegin, arg_iend};

        if (token.tag == tag::arg) {
          preprocess_tokens(arg_tokens, output, buffer);

        } else if (token.tag == tag::arg_str) {
          // estimate size
          size_t estim_size = 2;
          for (const auto& arg_tok : arg_tokens) estim_size += arg_tok.size;
          // create string in storage
          std::string& stringised = string_storage.emplace_back();
          // init string
          stringised.reserve(estim_size);
          // stringify all tokens
          stringised += '"';
          for (const auto& arg_tok : arg_tokens) {
            if (arg_tok.tag == tag::string_like_literal) {
              for (const char c : arg_tok.get_text()) {
                if (c == '"' || c == '\\') stringised += '\\';
                stringised += c;
              }
            } else {
              stringised += arg_tok.get_text();
            }
          }
          stringised += '"';
          // push token
          last_tag = tag::string_like_literal;
          buffer.push_back(
              make_token(stringised, last_tag, macro_token.start_pos));
          need_check = true;
        } else if (token.tag == tag::arg_raw) {
          // copy all tokens as are
          for (const auto& arg_tok : arg_tokens) buffer.push_back(arg_tok);
          if (!arg_tokens.empty()) last_tag = arg_tokens.back().tag;
          need_check = true;
        }
        continue;
      }

      if (need_check && incompatible(last_tag, token.tag)) {
        buffer.push_back(make_token(" ", last_tag, macro_token.start_pos));
      }
      need_check = false;

      token.start_pos.line = macro_token.start_pos.line;
      token.start_pos.column += macro_token.start_pos.column;
      token.end_pos.line += macro_token.start_pos.line;
      token.end_pos.column += macro_token.start_pos.column;
      buffer.push_back(token);
      last_tag = token.tag;
    }
    // clear applied args
    arg_rages.resize(arg_chunk.ibegin());

    push_process_macro(macro_token.get_text());
    {
      TokenListSlice buff_input{buffer, buffer_head};
      preprocess_tokens(buff_input, buffer, output);
      buffer.resize(buffer_head);
    }
    pop_process_macro(macro_token.get_text());
  }

  // true on final mark
  inline bool mark_arg_ranges(Tag tag, size_t index, int& balance) {
    switch (tag) {
      case tag::raw('('):
        ++balance;
        break;
      case tag::raw(','):
        if (balance == 1) arg_rages.push_back(index);
        break;
      case tag::raw(')'):
        --balance;
        if (balance != 0) break;
        arg_rages.push_back(index);
        return true;
      default:
        break;
    }
    return false;
  }

  inline void process_ppline(Tag tag) {
    switch (tag) {
      case tag::pp_include: {
        const IncludeTokenImage& includeImage =
            tokenImage.as<IncludeTokenImage>();
        break;
      }

      case tag::pp_define: {
        const DefineTokenImage& defineImage = tokenImage.as<DefineTokenImage>();
        macro_map.emplace(defineImage.name().get_text(),
                         compile_macro_expansion(defineImage));
        break;
      }

      case tag::pp_undef: {
        const UndefTokenImage& undefImage = tokenImage.as<UndefTokenImage>();
        macro_map.erase(undefImage.name().get_text());
        break;
      }

      default:
        break;
    }
  }
};
