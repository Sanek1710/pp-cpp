#pragma once

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
#include "helper.h"
#include "util/VectorTail.h"
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
                                const VectorTail<Token>& tokens) {
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

class Preprocessor {
  using IndexList = std::vector<size_t>;
  using IndexListTail = VectorTail<size_t>;
  using TokenListTail = VectorTail<Token>;

  using MacroMapValue = std::string;
  using MacroMapValuePtr = MacroMapValue*;

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
  SegStringMap<MacroMapValue> macroMap;

  TokenList out;
  TokenList buf;
  IndexList arg_rages;  // TokenList independent

  std::vector<std::string_view> macro_stack;

  DirectiveTokenImage tokenImage;

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
      if (!mark_arg_ranges(input[i].tag, i, balance, arg_chunk)) continue;
      const size_t nargs = arg_chunk.size() - 1;
      if (!macroStamp.is_valid_call(nargs)) break;
      return ++i;
    }
    // invalid macro use: eof
    arg_chunk.clear();
    return 0;
  }

  void preprocess_tokens(TokenListTail input, size_t size,  //
                         TokenList& buffer,  // same as input on start
                         TokenListTail output) {
    while (size) {
      switch (input.front().tag) {
        case tag::identifier: {
          const auto oMacroStamp = lookup_macro(input.front());
          if (!oMacroStamp) break;

          IndexListTail arg_chunk{arg_rages};
          const size_t nprescanned =
              prescan_macro(input, size, *oMacroStamp, arg_chunk);
          if (nprescanned == 0) break;
          expand_macro(*oMacroStamp, input, arg_chunk, buffer, output);
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

  // TODO: unify:

  size_t prescan_tkz_macro(Token token, Tokeniser& tkz,
                           TokenListTail output,  //
                           MacroStamp macroStamp, IndexListTail arg_chunk) {
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
    arg_chunk.push_back(output.size() - 1);

    int balance = 1;
    while (!tkz.eof()) {
      token = tkz.read_token();
      if (tag::is_ppline(token.tag)) {
        process_ppline(token.tag);
        continue;
      }
      output.push_back(token);
      if (!mark_arg_ranges(token.tag, output.size() - 1,  //
                           balance, arg_chunk))
        continue;
      const size_t nargs = arg_chunk.size() - 1;
      if (!macroStamp.is_valid_call(nargs)) break;
      return output.size();
    }
    // invalid macro use
    arg_chunk.clear();
    return 0;
  }

  void preprocess_tkz_tokens(Tokeniser& tkz, TokenWriter& writer) {
    out.clear();
    buf.clear();
    arg_rages.clear();
    macro_stack.clear();

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

          IndexListTail arg_chunk{arg_rages};
          const size_t nprescanned =
              prescan_tkz_macro(token, tkz, buf, *oMacroStamp, arg_chunk);
          if (nprescanned == 0) {
            writer.write(buf.front());
            preprocess_tokens(TokenListTail{buf, 1}, buf.size() - 1, buf, out);
          } else {
            expand_macro(*oMacroStamp, TokenListTail{buf, 0}, arg_chunk, buf,
                         out);
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

 private:
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
    auto macroIt = macroMap.find(macro_name);
    if (macroIt == macroMap.end()) {
      token.details.marker = true;
      return std::nullopt;
    }
    token.details.marker = false;
    if (in_process(macro_name)) return std::nullopt;
    return MacroStamp{macroIt->second};
  }

  void expand_macro(MacroStamp macro_stamp,                        //
                    TokenListTail input, IndexListTail arg_chunk,  //
                    TokenList& buffer,                             //
                    TokenListTail output) {
    TokenListTail preexpanion{buffer};
    const Token macro_token = input.front();
const char *a = STR(\\);
    MacroExpansionTokeniser macro_tkz{macro_stamp.expansion};
    while (!macro_tkz.eof()) {
      Token token = macro_tkz.read_token();
      if (token.tag == tag::arg) {
        const size_t arg_ibegin = arg_chunk[token.details.index] + 1;
        const size_t arg_iend = arg_chunk[token.details.index + 1];
        if (true /*not concat or stringify*/) {
          preprocess_tokens(input.slice(arg_ibegin), arg_iend - arg_ibegin,
                            output.base(), preexpanion);
        } else if (false /*stringify*/) {
          std::string stringised{'"'};
          for (const auto& arg_tok : IndexRange(input, arg_ibegin, arg_iend)) {
            for (const char c : arg_tok.get_text()) {
              if (c == '"') stringised += '\\';
              stringised += c;
            }
          }
          stringised += '"';
          // preexpanion.push_back(stringised);
          // stringify all tokens
        } else if (false /*concat*/) {
          for (const auto& arg_tok : IndexRange(input, arg_ibegin, arg_iend)) {
            preexpanion.push_back(arg_tok);
          }
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
    push_process_macro(macro_token.get_text());
    arg_chunk.clear();
    preprocess_tokens(preexpanion, preexpanion.size(), buffer, output);
    preexpanion.clear();
    pop_process_macro(macro_token.get_text());
  }

  // true on final mark
  inline bool mark_arg_ranges(Tag tag, size_t index, int& balance,
                              IndexListTail arg_chunk) {
    switch (tag) {
      case tag::raw('('): {
        ++balance;
        break;
      }
      case tag::raw(','): {
        if (balance == 1) arg_chunk.push_back(index);
        break;
      }
      case tag::raw(')'): {
        --balance;
        if (balance != 0) break;
        arg_chunk.push_back(index);
        return true;
      }
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
        macroMap.emplace(defineImage.name().get_text(),
                         compile_macro_expansion(defineImage));
        break;
      }

      case tag::pp_undef: {
        const UndefTokenImage& undefImage = tokenImage.as<UndefTokenImage>();
        macroMap.erase(undefImage.name().get_text());
        break;
      }

      default:
        break;
    }
  }
};
