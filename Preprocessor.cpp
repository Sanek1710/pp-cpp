
#include "Preprocessor.h"

#include <string_view>

#include "ExpansionTokeniser.h"
#include "util/indentos.h"
#include "util/rwfile.h"

namespace {

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

}  // namespace

void Preprocessor::fit_va_args(size_t expected, size_t actual) {
  const size_t last_index = arg_rages.back();
  auto begin = arg_rages.end() - actual;
  if (actual < expected) {
    --arg_rages.back();
    arg_rages.push_back(last_index);
  } else if (actual > expected) {
    arg_rages.erase(arg_rages.end() - actual + expected - 1, arg_rages.end());
    arg_rages.push_back(last_index);
  }
}

size_t Preprocessor::prescan_macro(TokenListSlice input,  //
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
    if (macroStamp.info.is_variadic) {
      fit_va_args(macroStamp.info.nargs, nargs);
    }
    return it + 1 - input.begin();
  }
  // invalid macro use: eof
  arg_rages.resize(args_head);
  return 0;
}

void Preprocessor::preprocess_tokens(
    TokenListSlice input,  //
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
    input.front().details.is_expansion = !macro_stack.empty();
    output.push_back(input.front());
    input.remove_prefix(1);
  }
}
size_t Preprocessor::prescan_tkz_macro(Token token, Tokeniser& tkz,
                                       TokenList& output,  //
                                       MacroStamp macroStamp) {
  output.clear();
  output.push_back(token);
  if (!macroStamp.info.is_functional) return output.size();

  // skip extras
  while (true) {
    if (tkz.eof()) return 0;
    token = read_token(tkz);
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
    token = read_token(tkz);
    if (tag::is_extra(token.tag)) {
      if (!tag::is_extra(output.back().tag))
        output.push_back(make_token(" ", tag::space, token.start_pos));
      continue;
    }

    output.push_back(token);
    if (!mark_arg_ranges(token.tag, output.size() - 1, balance)) continue;
    const size_t nargs = arg_rages.size() - args_head - 1;
    if (!macroStamp.is_valid_call(nargs)) break;
    if (macroStamp.info.is_variadic) {
      fit_va_args(macroStamp.info.nargs, nargs);
    }
    return output.size();
  }
  // invalid macro use
  arg_rages.resize(args_head);
  return 0;
}
void Preprocessor::preprocess_tkz_tokens(Tokeniser& tkz,
                                         TokenCodeWriter& writer) {
  while (!tkz.eof()) {
    Token token = read_token(tkz);

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
std::optional<MacroStamp> Preprocessor::lookup_macro(Token& token) const {
  if (token.details.is_not_macro) return std::nullopt;
  const auto macro_name = token.get_text();
  if (in_process(macro_name)) return std::nullopt;

  if (auto macro_it = macro_map.find(macro_name); macro_it != macro_map.end()) {
    token.details.is_not_macro = false;
    return MacroStamp{macro_it->second};
  }

  if (auto macro_it = context_macro_map.find(macro_name);
      macro_it != context_macro_map.end()) {
    token.details.is_not_macro = false;
    // add to local map?
    // macro_map.emplace(macro_it->first, macro_it->second);
    return MacroStamp{macro_it->second};
  }
  token.details.is_not_macro = true;
  return std::nullopt;
}

TokenListSlice Preprocessor::get_arg_range(TokenList& src,
                                           const IndexListSlice& arg_chunk,
                                           uint16_t arg_idx) {
  const size_t arg_ibegin = arg_chunk[arg_idx] + 1;
  const size_t arg_iend = arg_chunk[arg_idx + 1];
  TokenListSlice arg_range{src, arg_ibegin, arg_iend};
  // trim
  // by design is no more than one space
  if (!arg_range.empty() && tag::is_extra(arg_range.back().tag))
    arg_range.remove_suffix(1);
  if (!arg_range.empty() && tag::is_extra(arg_range.front().tag))
    arg_range.remove_prefix(1);

  return arg_range;
}

void Preprocessor::expand_macro(MacroStamp macro_stamp,  //
                                TokenListSlice input,
                                IndexListSlice arg_chunk,  //
                                TokenList& buffer,         //
                                TokenList& output) {
  TokenBuffWriter writer{buffer, string_storage};

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
      const IndexRange arg_tokens =
          get_arg_range(input.base(), arg_chunk, token.details.index);

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

  macro_stack.push_back(macro_token.get_text());
  preprocess_tokens(writer.as_input(), buffer, output);
  writer.clear();
  macro_stack.pop_back();
}

bool Preprocessor::mark_arg_ranges(Tag tag, size_t index, int& balance) {
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

void Preprocessor::prepreprocess(std::string& src) {
  auto orig_it = src.begin();
  auto it = src.begin();
  const auto end = src.end();

  for (; it != end; ++it) {
    if (*it == '\\') {
      auto next_it = it + 1;
      while (next_it != end && is_blank(*next_it)) ++next_it;
      if (*next_it == '\n') {
        it = next_it;
        offsets.push_back(orig_it.base());
        continue;
      }
    }
    *orig_it++ = *it;
  }
  src.erase(orig_it, src.end());
  last_newline_offset = src.begin().base();
  offsets_it = offsets.begin();
}

Token Preprocessor::read_token(Tokeniser& tkz) {
  Token token = tkz.read_token();
  while (tag::is_ppline(token.tag)) {
    process_ppline(tkz.tokenImage());
    token = tkz.read_token();
  }
  while (offsets_it != offsets.end() && token.start >= *offsets_it) {
    ++line_offset;
    last_newline_offset = *offsets_it++;
  }
  positer cur_newline_offset = token.start - token.start_pos.column;
  token.start_pos.line += line_offset;
  token.start_pos.column =
      token.start - std::max(last_newline_offset, cur_newline_offset);
  return token;
}

void Preprocessor::process_ppline(const DirectiveTokenImage& directive) {
  switch (directive.kind()) {
    case DirectiveTokenImage::Kind::Include: {
      directive.as_include();
      break;
    }

    case DirectiveTokenImage::Kind::Define: {
      macro_map.emplace(directive.as_define().name().get_text(),
                        compile_macro_expansion(directive.as_define()));
      break;
    }

    case DirectiveTokenImage::Kind::Undef: {
      macro_map.erase(directive.as_undef().name().get_text());
      break;
    }

    default:
      break;
  }
}

bool Preprocessor::in_process(std::string_view macro_name) const {
  return std::find(macro_stack.rbegin(), macro_stack.rend(), macro_name) !=
         macro_stack.rend();
}

// public interface:

void Preprocessor::process_code(std::string& src, std::string& output) {
  output.clear();
  clear();

  prepreprocess(src);
  write_file(ROOT "BCM/pre.out.cpp", src);

  TokenCodeWriter writer{output};
  Tokeniser tkz{src};
  preprocess_tkz_tokens(tkz, writer);
  notignore += writer.get_position_map().size();
}

void Preprocessor::clear() {
  macro_map.clear();
  string_storage.clear();
  out.clear();
  buf.clear();
  arg_rages.clear();
  macro_stack.clear();
  offsets.clear();
  line_offset = 0;
  last_newline_offset = 0;
  offsets_it = offsets.begin();
}
