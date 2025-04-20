#pragma once

#include "tkz/Position.h"
#include "util/util.h"

// Position manipulations

static_assert(sizeof(Position) == sizeof(uint64_t));

using PosStamp = uint64_t;

inline constexpr PosStamp toPosStamp(const Position& pos) {
  return (static_cast<uint64_t>(pos.line) << 32U) | pos.column;
}

inline constexpr bool operator==(const Position& pos1, const Position& pos2) {
  return toPosStamp(pos1) == toPosStamp(pos2);
}
inline constexpr bool operator!=(const Position& pos1, const Position& pos2) {
  return !(pos1 == pos2);
}
inline constexpr bool operator<(const Position& pos1, const Position& pos2) {
  return toPosStamp(pos1) < toPosStamp(pos2);
}

struct PositionHash {
  inline size_t operator()(const Position& pos) const noexcept {
    return toPosStamp(pos);
  }
};

using PositionMap = dense::map<Position, Position, PositionHash>;
