#pragma once

#include "Position.h"
#include "util.h"

inline constexpr bool operator==(const Position& pos1, const Position& pos2) {
  return pos1.line == pos2.line && pos1.column == pos2.column;
}
inline constexpr bool operator!=(const Position& pos1, const Position& pos2) {
  return !(pos1 == pos2);
}

struct PositionHash {
  static_assert(sizeof(Position) == sizeof(size_t));

  [[nodiscard]] auto operator()(Position pos) const noexcept -> uint64_t {
    return *reinterpret_cast<const uint64_t*>(&pos);
  }
};

using PositionMap = dense::map<Position, Position, PositionHash>;
