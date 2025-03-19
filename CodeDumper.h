#pragma once

#include <cstddef>
#include <string_view>

#include "PositionMap.h"
#include "Token.h"
#include "helper.h"
deadnote(int, naligned);
deadnote(int, nnotaligned);
class CodeDumper {
 public:
  // Pre-allocate output buffer
  explicit CodeDumper(std::string_view src) : src(src) {
    out.reserve(src.size());
  }

  // Fast path for aligned token dump
  // void dump_aligned(const Token& token) {
  //   const auto text = token.get_text(src);
  //   const auto text_size = text.size();

  //   // Fast path - single line token at expected position
  //   if (token.range.start_pos.line == last_pos.line &&
  //       token.range.start_pos.column == last_pos.column &&
  //       token.range.start_pos.line == token.range.end_pos.line) {
  //     out.append(text.data(), text_size);
  //     last_pos.column += text_size;
  //     return;
  //   }

  //   align_dump(token, src);
  // }

  // inline void dump(const Token& token) {
  //   const auto& pos = token.range.start_pos;
  //   if (pos == last_pos) {
  //     out += token.get_text(src);
  //     last_pos = token.range.end_pos;
  //     return;
  //   }
  //   posmap[last_pos] = token.range.start_pos;
  //   out += token.get_text(src);
  //   // applySameDelta(last_pos, token.range.start_pos, token.range.end_pos);
  //   addDeltaPos(last_pos, deltaPos(token.range));
  // }

  // Standard token dump with alignment
  inline void align_dump(const Token& token) {
    const auto& pos = token.range.start_pos;
    if (pos == last_pos) {
      store_cache(token);
      // out += token.get_text(src);
      last_pos = token.range.end_pos;
      return;
    }
    if (align_to(pos)) {
      out += token.get_text(src);
      last_pos = token.range.end_pos;
      getnote(naligned)++;
      return;
    }
    getnote(nnotaligned)++;
    posmap[last_pos] = pos;
    out += token.get_text(src);
    // applySameDelta(last_pos, token.range.start_pos, token.range.end_pos);
    addDeltaPos(last_pos, deltaPos(token.range));
  }

  // Raw text dump with alignment
  void align_dump(std::string_view code, Position pos) {
    dump_cache();
    if (!align_to(pos)) posmap[last_pos] = pos;
    insert(code);
  }

  // Access to internal buffer
  const std::string& get_output() {
    dump_cache();
    return out;
  }
  std::string&& take_output() {
    dump_cache();
    return std::move(out);
  }

  inline void putch(char c) {
    dump_cache();
    ++last_pos.column;
    out += c;
  }

 private:
  inline void store_cache(const Token& token) {
    if (cache_tok.range.end != token.range.start) {
      dump_cache();
      cache_tok = token;
      return;
    }
    cache_tok.range.end = token.range.end;
    cache_tok.range.end_pos = token.range.end_pos;
  }
  inline void dump_cache() {
    if (cache_tok.range.start == cache_tok.range.end) return;
    out += cache_tok.get_text(src);
    cache_tok.range.start = cache_tok.range.end;
    cache_tok.range.start_pos = cache_tok.range.end_pos;
  }

  inline void insert(std::string_view code) {
    for (char c : code) {
      ++last_pos.column;
      if (c == '\n') {
        ++last_pos.line;
        last_pos.column = 0;
      }
      out += c;
    }
  }

 private:
  inline void fill_spaces(uint32_t dlines, uint32_t dcols) {
    static constexpr const char* filler =
        "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"  //  16 '\n'
        "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"  //  32 '\n'
        "                                "  //  32 ' '
        "                                "  //  64 ' '
        "                                "  //  96 ' '
        "                                "  // 128 ' '
        ;
    static constexpr size_t nlines = 32U;
    static constexpr size_t ncols = 128U;

    static constexpr const char* filler_origin = filler + nlines;
    while (dlines > nlines) {
      out.append(filler, nlines);
      dlines -= nlines;
    }
    out.append(filler_origin - dlines, dlines + dcols % ncols);
    while (dcols > ncols) {
      out.append(filler_origin, ncols);
      dcols -= ncols;
    }
  }

  // inline bool align_to(Position pos) {
  //   if (last_pos.line == pos.line) {
  //     if (last_pos.column == pos.column) return true;
  //     if (last_pos.column > pos.column) return false;
  //     out.append(pos.column - last_pos.column, ' ');
  //     last_pos.column = pos.column;
  //     return true;
  //   }
  //   if (last_pos.line < pos.line) {
  //     out.append(pos.line - last_pos.line, '\n');
  //     out.append(pos.column, ' ');
  //     // fill_spaces(pos.line - last_pos.line, pos.column);
  //     last_pos = pos;
  //     return true;
  //   }
  //   return false;
  // }

  inline bool align_to(const Position& pos) {
    if (last_pos == pos) return true;
    if (pos < last_pos) return false;
    const auto dpos = deltaPos(last_pos, pos);
    out.append(dpos.line, '\n');
    out.append(dpos.column, ' ');
    // fill_spaces(dpos.line,dpos.column);
    last_pos = pos;
    return true;
  }

  //  private:
  Position last_pos;
  std::string out;
  std::string_view src;
  Token cache_tok;
  PositionMap posmap;
};
