#pragma once

#include <string>
#include <string_view>
#include "Position.h"
#include "Token.h"
#include "PositionMap.h"

class CodeDumper {
public:
  // Pre-allocate output buffer
  explicit CodeDumper(size_t reserve_size = 0) { 
    out.reserve(reserve_size);
  }

  // Fast path for aligned token dump
  void dump_aligned(const Token& token, std::string_view src) {
    const auto text = token.get_text(src);
    const auto text_size = text.size();
    
    // Fast path - single line token at expected position
    if (token.range.start_pos.line == last_pos.line && 
        token.range.start_pos.column == last_pos.column &&
        token.range.start_pos.line == token.range.end_pos.line) {
      out.append(text.data(), text_size);
      last_pos.column += text_size;
      return;
    }

    align_dump(token, src);
  }

  // Standard token dump with alignment
  void align_dump(const Token& token, std::string_view src) {
    const auto& pos = token.range.start_pos;
    if (!align_to(pos)) {
      posmap[last_pos] = pos;
    }

    out += token.get_text(src);
    const auto dline = token.range.end_pos.line - token.range.start_pos.line;
    last_pos.line += dline;
    if (dline == 0) {
      last_pos.column += token.range.end_pos.column - token.range.start_pos.column;
    } else {
      last_pos.column = token.range.end_pos.column;
    }
  }

  // Raw text dump with alignment
  void align_dump(std::string_view code, Position pos) {
    if (!align_to(pos)) {
      posmap[last_pos] = pos;
    }
    insert(code);
  }

  // Access to internal buffer
  const std::string& get_output() const { return out; }
  std::string&& take_output() { return std::move(out); }

private:
  bool align_to(Position pos) {
    if (last_pos.line == pos.line) {
      if (last_pos.column == pos.column) return true;
      if (last_pos.column > pos.column) return false;
      out.append(pos.column - last_pos.column, ' ');
      last_pos.column = pos.column;
      return true;
    }
    if (last_pos.line < pos.line) {
      out.append(pos.line - last_pos.line, '\n');
      out.append(pos.column, ' ');
      last_pos = pos;
      return true;
    }
    return false;
  }

  void insert(std::string_view code) {
    for (char c : code) {
      ++last_pos.column;
      if (c == '\n') {
        ++last_pos.line;
        last_pos.column = 0;
      }
      out += c;
    }
  }

public:
  Position last_pos;
  std::string out;
  PositionMap posmap;
}; 