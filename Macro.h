#pragma once

#include <algorithm>
#include <charconv>
#include <string_view>

#include "Token.h"
#include "TokenGroup.h"

using FileID = unsigned;

struct Marker {
  enum {
    as_is = '_',
    stringify = 's',
    spacing = ' ',
    nothing = '.',
  };
};

// Helper struct to parse macro information
struct MacroStamp {
  MacroStamp(std::string_view content) {
    auto it = content.begin();
    info.is_variadic = *it == 'v';
    info.is_functional = *it != ' ';
    if (info.is_functional) {
      it = std::from_chars(++it, content.end(), info.nargs).ptr;
    }
    ++it;
    expansion = std::string_view(it, content.end() - it);
  }

  bool is_valid_call(unsigned short nargs_input) {
    return info.is_variadic ? info.nargs - 1 <= nargs_input
                            : info.nargs == nargs_input;
  }

  std::string_view expansion;
  MacroInfo info;
};

inline std::string compile_macro_expansion(const DefineTokenImage& macro) {
  std::string out;

  // Write macro type prefix
  if (macro.info().is_functional) {
    out += macro.info().is_variadic ? 'v' : 'f';
    out += std::to_string(macro.info().nargs);
  }
  out += ' ';

  bool need_space = false;
  Tag last_tag = tag::other;

  const auto args = macro.args_view();
  for (const Token& tok : macro.expansion_view()) {
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
    if (macro.info().is_functional && tok.tag == tag::identifier) {
      const auto text = tok.get_text();

      // check arg
      auto arg_it = std::find_if(
          args.begin(), args.end(),
          [&](const Token& arg) { return arg.get_text() == text; });

      // or __VA_ARGS__ if expected
      if (macro.info().is_variadic && text == "__VA_ARGS__" &&
          args.back().get_text().empty()) {
        arg_it = std::prev(args.end());
      }

      // actual arg
      if (arg_it != args.end()) {
        out += '$';
        out += std::to_string(arg_it - args.begin());
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

    for (char c : tok.get_text()) {
      if (c == '$') out += '$';
      out += c;
    }
    last_tag = tok.tag;
  }

  // Clean up trailing space
  // if (!out.empty() && out.back() == ' ') out.pop_back();
  return out;
}
