#pragma once

#include <cstddef>
#include <string_view>

#include "PositionMap.h"
#include "Token.h"
#include "helper.h"
deadnote(int, naligned);
deadnote(int, nnotaligned);
deadnote(int, ncached);

class CodeDumper {
 public:
  // Pre-allocate output buffer
  explicit CodeDumper(std::string_view src) : src(src) {
    out.reserve(src.size());
  }

  // Standard token dump with alignment
  inline void align_dump(const Token& token) {
    const auto& pos = token.start_pos;
    if (align_to(pos)) {
      out += token.get_text();
      last_pos = token.end_pos;
      getnote(naligned)++;
      return;
    }
    getnote(nnotaligned)++;
    posmap[last_pos] = pos;
    out += token.get_text();
    // applySameDelta(last_pos, token.start_pos, token.end_pos);
    addDeltaPos(last_pos, deltaPos(token.start_pos, token.end_pos));
  }

  // Raw text dump with alignment
  void align_dump(std::string_view code, Position pos) {
    if (!align_to(pos)) posmap[last_pos] = pos;
    insert(code);
  }

  // Access to internal buffer
  const std::string& get_output() const { return out; }
  std::string&& take_output() { return std::move(out); }

  inline void putch(char c) {
    ++last_pos.column;
    out += c;
  }

 private:
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
