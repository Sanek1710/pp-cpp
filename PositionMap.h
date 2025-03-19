#pragma once

#include "Position.h"
#include "util.h"

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

// Range manipulations
inline Position deltaPos(const Position& less, const Position& greater) {
  Position dpos;
  dpos.line = greater.line - less.line;
  dpos.column = dpos.line ? greater.column : greater.column - less.column;
  return dpos;
}

inline Position deltaPos(const Range& range) {
  return deltaPos(range.start_pos, range.end_pos);
}

inline void addDeltaPos(Position& pos, const Position& dpos) {
  pos.line += dpos.line;
  pos.column = dpos.line ? 0 : pos.column;
  pos.column += dpos.column;
}

inline void applySameDelta(Position& orig, const Position& less,
                           const Position& greater) {
  const auto dline = greater.line - less.line;
  orig.line += dline;
  if (dline) {
    orig.column = greater.column;
  } else {
    orig.column += greater.column - less.column;
  }
}