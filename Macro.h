#pragma once

#include <algorithm>
#include <charconv>

#include "Token.h"
#include "TokenGroup.h"

using FileID = unsigned;

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

// Compile macro definition into a pattern with argument processing markers
inline std::string compile_macro_expansion(const DefineImage& macro,
                                           std::string_view src) {
  std::string out;

  // Write macro type prefix
  if (macro.info.is_functional) {
    out += macro.info.is_variadic ? 'v' : 'f';
    out += std::to_string(macro.info.nargs);
  }
  out += ' ';

  bool need_space = false;
  Tag last_tag = tag::other;

  auto it = macro.expansion_begin();
  const auto end = macro.expansion_end();
  for (; it != end; ++it) {
    const Token& tok = *it;

    // skip extras mark as need space
    if (tag::is_extra(tok.tag)) {
      if (last_tag != tag::pp_op_cat  //
          && last_tag != tag::pp_op_str)
        need_space = true;
      continue;
    }

    if (tok.tag == tag::pp_op_cat) {
      if (last_tag == tag::arg) {
        if (out.back() == ' ') out.back() = '_';
      }
      need_space = false;
      last_tag = tag::pp_op_cat;
      continue;
    }

    if (tok.tag == tag::pp_op_str) {
      if (last_tag != tag::pp_op_cat) need_space = true;
      last_tag = tag::pp_op_str;
      continue;
    }

    if (need_space && out.back() != ' ') out += ' ';
    need_space = false;

    // what if arg
    if (macro.info.is_functional && tok.tag == tag::identifier) {
      const auto text = tok.get_text(src);

      // check arg
      auto arg_it = std::find_if(
          macro.args_begin(), macro.args_end(),
          [&](const Token& arg) { return arg.get_text(src) == text; });

      // or __VA_ARGS__ if expected
      if (macro.info.is_variadic && text == "__VA_ARGS__" &&
          macro.args_back().get_text(src).empty()) {
        arg_it = std::prev(macro.args_end());
      }

      // actual arg
      if (arg_it != macro.args_end()) {
        out += '$';
        out += std::to_string(arg_it - macro.args_begin());
        if (last_tag == tag::pp_op_str) {
          out += 's';
        } else if (last_tag == tag::pp_op_cat) {
          out += '_';
        } else {
          out += ' ';
        }
        last_tag = tag::arg;
        continue;
      }
    }

    for (char c : tok.get_text(src)) {
      if (c == '$') out += '$';
      out += c;
    }
    last_tag = tok.tag;
  }

  // Clean up trailing space
  // if (!out.empty() && out.back() == ' ') out.pop_back();
  return out;
}
