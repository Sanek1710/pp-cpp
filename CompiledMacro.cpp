
#include "CompiledMacro.h"

#include <charconv>
#include <string_view>

#include "tkz/TokenGroup.h"

namespace {

static constexpr Tag pp_op_to_arg_tag(Tag op_tag) {
  switch (op_tag) {
    case tag::pp_op_str:
      return tag::arg_str;
    case tag::pp_op_cat:
      return tag::arg_raw;
    default:
      break;
  }
  return tag::arg;
};

}  // namespace

CompiledMacro::CompiledMacro(const DefineView& macro) {
  static constexpr std::string_view VA_ARGS_NAME = "__VA_ARGS__";

  // TODO: is that even needed?
  // just add info to all the structures?
  if (macro.info().is_functional) {
    compiled += macro.info().is_variadic ? variadic_marker : functional_marker;
    compiled += std::to_string(macro.info().nargs);
  }
  compiled += info_delimiter;

  bool need_space = false;
  Tag last_tag = tag::space;

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
        if (compiled.back() == tag::markerof(tag::arg))
          compiled.back() = tag::markerof(tag::arg_raw);
      }
      need_space = false;
      last_tag = tag::pp_op_cat;
      compiled += "##";
      continue;
    }

    if (tok.tag == tag::pp_op_str) {
      if (last_tag != tag::pp_op_cat) need_space = true;
      last_tag = tag::pp_op_str;
      continue;
    }

    if (need_space && compiled.back() != ' ') compiled += ' ';
    need_space = false;

    // what if arg
    if (macro.info().is_functional && tok.tag == tag::identifier) {
      const auto text = tok.get_text();

      // check arg
      auto arg_it = std::find_if(
          args.begin(), args.end(),
          [&](const Token& arg) { return arg.get_text() == text; });

      // or __VA_ARGS__ if expected
      if (macro.info().is_variadic && text == VA_ARGS_NAME &&
          args.back().get_text().empty()) {
        arg_it = std::prev(args.end());
      }

      // actual arg
      if (arg_it != args.end()) {
        compiled += '$';
        compiled += std::to_string(arg_it - args.begin());
        last_tag = pp_op_to_arg_tag(last_tag);
        compiled += tag::markerof(last_tag);
        continue;
      }
    }

    if (tok.tag == tag::raw('$')) compiled += '$';
    compiled += tok.get_text();
    last_tag = tok.tag;
  }
}

MacroStamp CompiledMacro::get_stamp() const {
  MacroInfo info;
  std::string_view compiled_view = raw();
  auto it = compiled_view.begin();
  info.is_variadic = *it == variadic_marker;
  info.is_functional = *it != info_delimiter;
  if (info.is_functional) {
    it = std::from_chars(++it, compiled_view.end(), info.nargs).ptr;
  }
  ++it;
  return MacroStamp{.expansion = std::string_view(it, compiled_view.end() - it),
                    .info = info};
}
