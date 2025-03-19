#pragma once

#include <cstdint>
#include <ostream>

struct Position {
  uint32_t line = 0;
  uint32_t column = 0;
};

struct Range {
  uint32_t start = 0;
  uint32_t end = 0;
  Position start_pos;
  Position end_pos;
};

std::ostream& operator<<(std::ostream& os, const Position& pos);
std::ostream& operator<<(std::ostream& os, const Range& range);
