#pragma once

#include <algorithm>
#include <charconv>

#include "Token.h"
#include "TokenGroup.h"


// Helper struct to parse macro information
struct MacroStamp {
  MacroStamp(std::string_view content) {
    auto it = content.begin();
    info.is_functional = *it != ' ';
    info.is_variadic = *it == 'v';
    auto res = std::from_chars(++it, content.end(), info.nargs);
    expansion = std::string_view(res.ptr, content.end() - res.ptr);
  }

  std::string_view expansion;
  MacroInfo info;
};

// Compile macro expansion into internal format
inline std::string compile_macro_expansion(const DefineImage& macroIm,
                                           std::string_view src) {
  std::string out;
  if (macroIm.info.is_functional) {
    out += macroIm.info.is_variadic ? 'v' : 'f';
    out += std::to_string(macroIm.info.nargs);
  }
  out += ' ';
  auto prefix_size = out.size();

  bool was_arg = false;
  char nxt_arg_appl = ' ';
  for (auto expIt = macroIm.expansion_begin(); expIt != macroIm.expansion_end();
       ++expIt) {
    const Token& tok = *expIt;
    char arg_appl = nxt_arg_appl;
    nxt_arg_appl = ' ';

    if (tok.id == Token::pp_op_cat) {
      if (was_arg && out.back() == ' ') out.back() = '#';
      nxt_arg_appl = '#';
      continue;
    }
    was_arg = false;

    if (tok.id == Token::pp_op_str) {
      if (out.back() != ' ') out += ' ';
      nxt_arg_appl = 's';
      continue;
    }

    if (is_extra(tok.id)) {
      if (out.back() != ' ') out += ' ';
      continue;
    }
    const auto text = tok.get_text(src);

    if (macroIm.info.is_functional && tok.id == Token::identifier) {
      auto argIt = std::find_if(macroIm.args_begin(), macroIm.args_end(),
                                [src, text](const Token& arg_tok) {
                                  return arg_tok.get_text(src) == text;
                                });
      if (macroIm.info.is_variadic && text == "__VA_ARGS__") {
        if (!macroIm.args_back().get_text(src).empty()) {
          out += text;
          continue;
        }
        argIt = macroIm.args_end() - 1;
      }
      if (argIt != macroIm.args_end()) {
        out += '$';
        out += std::to_string(argIt - macroIm.args_begin());
        out += arg_appl;
        was_arg = arg_appl != 's';
        if (!was_arg) out += ' ';
        continue;
      }
      out += text;
      continue;
    }

    for (char c : text) {
      if (c == '$') out += '$';
      out += c;
    }
  }
  while (out.size() > prefix_size && out.back() == ' ') out.pop_back();
  return out;
}
