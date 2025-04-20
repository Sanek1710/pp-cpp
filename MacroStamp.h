#pragma once

#include <algorithm>
#include <charconv>
#include <string_view>

#include "tkz/Token.h"
#include "tkz/TokenGroup.h"

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

  std::string_view expansion;
  // FileID fileId;
  MacroInfo info;
};

inline constexpr Tag pp_op_to_arg_tag(Tag op_tag) {
  switch (op_tag) {
    case tag::pp_op_str:
      return tag::arg_str;
    case tag::pp_op_cat:
      return tag::arg_raw;
    default:
      break;
  }
  return tag::arg;
}

inline std::string compile_macro_expansion(const DefineView& macro) {
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
      if (tag::is_macro_arg(last_tag)) {
        if (out.back() == tag::markerof(tag::arg))
          out.back() = tag::markerof(tag::arg_raw);
      }
      need_space = false;
      last_tag = tag::pp_op_cat;
      out += "##";
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
        last_tag = pp_op_to_arg_tag(last_tag);
        out += tag::markerof(last_tag);
        continue;
      }
    }

    if (tok.tag == tag::raw('$')) out += '$';
    out += tok.get_text();
    last_tag = tok.tag;
  }
  // if (need_space && out.back() != ' ' && tag::is_macro_arg(last_tag))
  //   out += ' ';
  // Clean up trailing space
  // if (!out.empty() && out.back() == ' ') out.pop_back();
  return out;
}
