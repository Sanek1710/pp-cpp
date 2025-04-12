#pragma once

#include <algorithm>
#include <cstddef>
#include <ios>
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

class TokenCodeWriter {
 public:
  TokenCodeWriter(std::string& out) : out(out) {}

  inline void write(const Token& token) {
    if (tag::is_extra(token.tag)) return putspace();

    if (!align(token.start_pos, !token.details.is_expansion)) {
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
    if (needs_space(token)) putspace();
    // raw is always one non newline symbol
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
      out += c;
    }

    last_tag = token.tag;

    // dumb logic with position difference:
    // addDeltaPos(last_pos, deltaPos(token.start_pos, token.end_pos));
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

  bool needs_space(const Token& token) const {
    static constexpr auto oneof = [](char c, std::string_view char_set) {
      return char_set.rfind(c) != std::string_view::npos;
    };

    if (last_tag == tag::space || last_tag == tag::space) return false;

    const char crhs = token.get_text().front();
    // anything after categorised chars
    if (!tag::is_raw(last_tag) && !tag::is_punct(last_tag)) {
      // in most cases these are iconpatible
      if (!tag::is_raw(token.tag) && !tag::is_punct(token.tag)) return true;
      return last_tag == tag::number && oneof(crhs, ".+-");
    }

    // categorised chars after raw chars
    if (!tag::is_raw(token.tag) && !tag::is_punct(token.tag))
      return (last_tag == tag::raw('.') || last_tag == tag::ellipsis) &&
             token.tag == tag::number;

    // raw chars after raw chars

    if (last_tag == tag::punct2) {
      std::string_view last_text{out.data() + out.size() - 2, 2};
      if (last_text == "->" && crhs == '*') return true;
      if ((last_text == ">>" || last_text == "<<") && crhs == '=') return true;
      return false;
    }

    // last_tag is raw
    const char clhs = tag::markerof(last_tag);
    switch (crhs) {
      // clang-format off
      case '#': return clhs == '#'; // ppline
      case '%': return oneof(clhs, ".<");
      case '&': return clhs == '&';
      case '*': return clhs == '/';
      case '+': [[fallthrough]];
      case '-': [[fallthrough]];
      case '.': [[fallthrough]];
      case '/': return clhs == crhs;
      case ':': return oneof(clhs, ":%<");
      case '<': return clhs == '<';
      case '=': return oneof(clhs, "!%&*+-/<=>|^");
      case '>': return oneof(clhs, ":%->");
      case '|': return clhs == '|';
      // clang-format on
      default:
        break;
    }
    // rest do not make any known operators
    return false;
  }
};
