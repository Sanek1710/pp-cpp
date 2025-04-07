#pragma once

#include <algorithm>
#include <cstddef>
#include <iomanip>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "CodeDumper.h"
#include "Cursor.h"
#include "ExpansionTokeniser.h"
#include "Macro.h"
#include "Token.h"
#include "TokenGroup.h"
#include "TokenPrinter.h"
#include "ankerl/unordered_dense.h"
#include "util/RangeView.h"
#include "util/helper.h"
#include "util/util.h"

// #define debug
#ifdef debug
#define DBG(...) __VA_ARGS__
#else
#define DBG(...)
#endif

constexpr Token make_token(std::string_view text, Tag tag, Position start_pos) {
  return Token{.tag = tag,
               .details = {0},
               .size = static_cast<uint32_t>(text.size()),
               .start = text.data(),
               .start_pos = start_pos,
#ifdef ENDPOS
               .end_pos = start_pos
#endif
  };
}

constexpr Token empty_token(Position start_pos) {
  return Token{.tag = tag::empty,
               .details = {0},
               .size = 0,
               .start = "",
               .start_pos = start_pos,
#ifdef ENDPOS
               .end_pos = start_pos
#endif
  };
}

inline Token cat_tokens(Token lhs, Token rhs, std::string& text) {
  text += lhs.get_text();
  text += rhs.get_text();
  // TODO: deduce tag from both tags
  constexpr const auto cat_tags = [](Tag lhs, Tag rhs) { return lhs; };
  return Token{.tag = cat_tags(lhs.tag, rhs.tag),
               .details = {0},
               .size = static_cast<uint32_t>(text.size()),
               .start = text.data(),
               .start_pos = lhs.start_pos,
#ifdef ENDPOS
               .end_pos = rhs.end_pos
#endif
  };
}

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
  TokenWriter(CodeDumper& dumper) : dumper(dumper) {}
  void write(Token tok, bool align = true) {
    if (align) {
      dumper.align_dump(tok);
    } else {
      dumper.dump(tok);
    }
    last_tag = tok.tag;
    ++ntokens;
    // output += tok.get_text();
    // output.push_back(tok);
  }
  size_t size() const { return ntokens; }

  Tag last_tag = tag::space;

 private:
  CodeDumper& dumper;
  size_t ntokens = 0;
};

using TokenListSlice = IndexRange<TokenList>;
using StringStorage = dense::segmented_vector<std::string>;
class TokenBuffWriter {
 public:
  TokenBuffWriter(TokenList& buffer, StringStorage& string_storage)
      : buffer(buffer), head(buffer.size()), string_storage(string_storage) {}

  void write(Token tok) {
    if (cat_mode) {
      if (buffer.size() > head)
        buffer.back() =
            cat_tokens(buffer.back(), tok, string_storage.emplace_back());
      cat_mode = false;
      return;
    }
    buffer.push_back(tok);
    last_tag = tok.tag;
  }

  TokenListSlice as_input() const { return {buffer, head}; }
  inline void clear() const { buffer.resize(head); }
  inline void set_cat(bool value = true) { cat_mode = true; }

  Tag last_tag = tag::space;

 private:
  TokenList& buffer;
  size_t head;
  StringStorage& string_storage;
  bool cat_mode = false;
};

inline std::ostream& operator<<(std::ostream& os,
                                const IndexRange<std::vector<Token>>& tokens) {
  TokenPrinter printer{os, false};
  os << "|";
  for (const auto& token : tokens) {
    if (token.tag == tag::newline) {
      os << " \\n ";
    } else {
      printer.print(token);
    }
  }
  return os;
}

class Preprocessor {
  using IndexList = std::vector<size_t>;

  using IndexListSlice = IndexRange<IndexList>;

  using MacroMapValue = std::string;
  using MacroMapValuePtr = MacroMapValue*;

 public:
  std::string output;
  const auto& process_code(std::string_view src) {
    Tokeniser tkz{src, tokenImage};

    output.clear();
    CodeDumper dumper{output};
    TokenWriter writer{dumper};
    preprocess_tkz_tokens(tkz, writer);
    dumper.finalise();

    return output;
  }

  const auto& process_code(std::string_view src, std::string& output) {
    Tokeniser tkz{src, tokenImage};

    output.clear();
    CodeDumper dumper{output};
    TokenWriter writer{dumper};
    preprocess_tkz_tokens(tkz, writer);
    dumper.finalise();

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

  inline void clear() {
    macro_map.clear();
    string_storage.clear();
    out.clear();
    buf.clear();
    arg_rages.clear();
    macro_stack.clear();
  }

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
      return it + 1 - input.begin();
    }
    // invalid macro use: eof
    arg_rages.resize(args_head);
    return 0;
  }

  void preprocess_tokens(TokenListSlice input,  //
                         TokenList& buffer,     // same as input on start
                         TokenList& output) {
    DBG(indentos ios{std::cerr};)
    size_t dbg_buf_head = buffer.size();
    size_t dbg_out_head = out.size();
    DBG(std::cerr << "preprocess_tokens: " << input << "\n";)
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
          DBG(std::cerr << "removed prefix: " << input << "\n";)
          continue;
        }
        default:
          break;
      }
      output.push_back(input.front());
      input.remove_prefix(1);
    }

    DBG(std::cerr << (&buffer == &out ? "->" : "  ");
        std::cerr << "buffer: " << IndexRange(buffer, 0, dbg_buf_head)
                  << IndexRange(buffer, dbg_buf_head) << "\n";
        std::cerr << (&output == &out ? "->" : "  ");
        std::cerr << "output: " << IndexRange(output, 0, dbg_out_head)
                  << IndexRange(output, dbg_out_head) << "\n";)
  }

  // TODO: unify:

  size_t prescan_tkz_macro(Token token, Tokeniser& tkz,
                           TokenList& output,  //
                           MacroStamp macroStamp) {
    output.clear();
    output.push_back(token);
    if (!macroStamp.info.is_functional) return output.size();

    // skip extras
    while (true) {
      if (tkz.eof()) return 0;
      token = tkz.read_token();
      if (tag::is_ppline(token.tag)) {
        process_ppline(token.tag);
        continue;
      }
      if (!tag::is_extra(token.tag)) break;
    }
    output.push_back(token);
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
      if (tag::is_extra(token.tag)) {
        if (!tag::is_extra(output.back().tag))
          output.push_back(make_token(" ", tag::space, token.start_pos));
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
    clear();

    while (!tkz.eof()) {
      Token token = tkz.read_token();
      if (tag::is_ppline(token.tag)) {
        process_ppline(token.tag);
        continue;
      }

      if (tag::is_extra(token.tag)) continue;

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
            // writer.write(empty_token(token.start_pos));
          }
          notignore += buf.size();
          notignore += out.size();
          buf.clear();
          for (const auto& token : out) writer.write(token, false);
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

  IndexRange<TokenList> get_arg_range(TokenList& src,
                                      const IndexRange<IndexList>& arg_chunk,
                                      uint16_t arg_idx) {
    const size_t arg_ibegin = arg_chunk[arg_idx] + 1;
    const size_t arg_iend = arg_chunk[arg_idx + 1];
    IndexRange arg_range{src, arg_ibegin, arg_iend};
    // trim
    // by design is no more than one space
    if (!arg_range.empty() && tag::is_extra(arg_range.back().tag))
      arg_range.remove_suffix(1);
    if (!arg_range.empty() && tag::is_extra(arg_range.front().tag))
      arg_range.remove_prefix(1);

    return arg_range;
  }

  void expand_macro(MacroStamp macro_stamp,                          //
                    TokenListSlice input, IndexListSlice arg_chunk,  //
                    TokenList& buffer,                               //
                    TokenList& output) {
    DBG(indentos ios{std::cerr};)
    TokenBuffWriter writer{buffer, string_storage};
    DBG(std::cerr << "expand: " << input << "\n";)

    const Token macro_token = input.front();

    MacroExpansionTokeniser macro_tkz{macro_stamp.expansion,
                                      macro_token.start_pos};
    while (!macro_tkz.eof()) {
      Token token = macro_tkz.read_token();
      // check catenation

      if (token.tag == tag::pp_op_cat) {
        writer.set_cat();
        continue;
      }

      if (tag::is_macro_arg(token.tag)) {
        DBG(indentos ios{std::cerr};)
        const IndexRange arg_tokens =
            get_arg_range(input.base(), arg_chunk, token.details.index);

        DBG(std::cerr << "arg:\n";)
        if (token.tag == tag::arg) {
          preprocess_tokens(arg_tokens, output, buffer);
        } else if (token.tag == tag::arg_str) {
          // create string in storage
          std::string& stringised = string_storage.emplace_back();
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
          writer.write(make_token(stringised, tag::string_like_literal,
                                  macro_token.start_pos));
        } else if (token.tag == tag::arg_raw) {
          // copy all tokens as are
          for (const auto& arg_tok : arg_tokens) writer.write(arg_tok);
        }
        continue;
      }
      writer.write(token);
    }
    // clear applied args
    arg_rages.resize(arg_chunk.ibegin());

    push_process_macro(macro_token.get_text());
    {
      DBG(std::cerr << "postexpand: " << writer.as_input() << "\n";)
      preprocess_tokens(writer.as_input(), buffer, output);
      writer.clear();
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
        tokenImage.as_include();
        break;
      }

      case tag::pp_define: {
        macro_map.emplace(tokenImage.as_define().name().get_text(),
                          compile_macro_expansion(tokenImage.as_define()));
        break;
      }

      case tag::pp_undef: {
        macro_map.erase(tokenImage.as_undef().name().get_text());
        break;
      }

      default:
        break;
    }
  }
};
