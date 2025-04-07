#pragma once

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <string_view>
#include <utility>
#include <vector>

#include "Position.h"
#include "PositionMap.h"
#include "Token.h"
#include "ankerl/unordered_dense.h"

#include "util/helper.h"

deadnote(int, naligned);
deadnote(int, nnotaligned);
deadnote(int, ncached);
deadnote(int, notequal);

// TODO: rewrite it to accept operator things
inline constexpr bool incompatible(Tag lhs, Tag rhs) {
  if (lhs == tag::space || rhs == tag::space) return false;
  const char mrhs = tag::markerof(rhs);
  // anything after categorised chars
  if (!tag::is_raw(lhs)) {
    // in most cases these are iconpatible
    if (!tag::is_raw(rhs)) return true;
    return lhs == tag::number && oneof(mrhs, ".+-");
  }

  // categorised chars after raw chars
  if (!tag::is_raw(rhs)) return lhs == tag::raw('.') && rhs == tag::number;

  // raw chars after raw chars
  const char mlhs = tag::markerof(lhs);
  return cats_operator(mlhs, mrhs);
}

class CodeDumper {
 public:
  CodeDumper(std::string& out) : out(out) {}

  // standard token dump with alignment
  inline void align_dump(const Token& token) {
    if (token.get_text() == "is_digit") {
      std::cerr << token.get_text();
    }
    if (tag::is_extra(token.tag)) return putspace();

    // getnote(szstat).stats[token.size]++;
    if (!align(token.start_pos, true)) {
      getnote(nnotaligned)++;
      if (token.tag == tag::identifier) {
        map_last_pos(token.start_pos);
      }
    } else {
      getnote(naligned)++;
    }

    insert(token);
  }

  inline void dump(const Token& token) {
    if (tag::is_extra(token.tag)) return putspace();
    bool align_column = false;
    if (!align(token.start_pos, align_column)) {
      getnote(nnotaligned)++;
      if (token.tag == tag::identifier) {
        map_last_pos(token.start_pos);
      }
    } else {
      getnote(naligned)++;
    }

    insert(token);
  }

  inline void putspace() {
    if (last_tag == tag::space) return;
    out += ' ';
    ++last_pos.column;
    last_tag = tag::space;
  }

  inline void putraw(Tag tag) {
    out += tag::markerof(tag);
    ++last_pos.column;
    last_tag = tag;
  }

  inline void put_plain(char c) {
    ++last_pos.column;
    out += c;
  }

  inline void finalise() {
    posmap.reserve(pmap.capacity());
    posmap.insert(pmap.begin(), pmap.end());
    notignore += posmap.bucket_count();
  }

  inline void map_last_pos(Position pos) {
    getnote(ncached)++;
    pmap.emplace_back(last_pos, pos);
    // posmap.emplace(last_pos, pos);

    // posmap.emplace_back(last_pos, pos);
  }

 private:
  Position last_pos;
  std::string& out;
  Token cache_tok;
  Tag last_tag = tag::space;
  PositionMap posmap;
  std::vector<std::pair<Position, Position>> pmap;

  inline void insert(const Token& token) {
    // raw is always one non newline symbol
    if (incompatible(last_tag, token.tag)) putspace();
    if (tag::is_raw(token.tag)) return putraw(token.tag);
    // if (token.tag == tag::operator2) {
    //   out += token.get_text();
    //   last_pos.column += 2;
    //   last_tag = token.tag;
    //   return;
    // }

    // the most easiest way to do it correctly
    // slightly slower than smart logic with position differences
    for (char c : token.get_text()) {
      ++last_pos.column;
      if (c == '\n') {
        ++last_pos.line;
        last_pos.column = 0;
      }
    }

    out += token.get_text();
    last_tag = token.tag;

    // dumb logic with position difference:
    // addDeltaPos(last_pos, deltaPos(token.start_pos, token.end_pos));
  }

  inline bool align_pos(const Position& pos) {
    if (last_pos == pos) return true;
    if (pos < last_pos) return false;
    const auto dpos = deltaPos(last_pos, pos);
    out.append(dpos.line, '\n');
    out.append(dpos.column, ' ');
    last_pos = pos;
    return true;
  }

  inline bool align_line(const Position& pos) {
    if (last_pos.line > pos.line) return false;
    const uint32_t dline = pos.line - last_pos.line;
    if (dline != 0) {
      out.append(dline, '\n');
      last_pos.line = pos.line;
      last_pos.column = 0;
    }
    return last_pos.column == pos.column;
  }

  inline bool align(const Position& pos, bool align_column) {
    if (last_pos.line > pos.line) return false;
    const uint32_t dline = pos.line - last_pos.line;
    if (dline != 0) {
      out.append(dline, '\n');
      last_tag = tag::space;
      last_pos.line = pos.line;
      last_pos.column = 0;
    }
    if (last_pos.column == pos.column) return true;
    if (!align_column) return false;
    if (last_pos.column > pos.column) return false;
    out.append(pos.column - last_pos.column, ' ');
    last_tag = tag::space;
    last_pos.column = pos.column;
    return true;
  }
};
