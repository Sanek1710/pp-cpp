#pragma once

#include <cstdint>
#include <ostream>

struct Position {
  uint32_t line = 0;
  uint32_t column = 0;
};

std::ostream& operator<<(std::ostream& os, const Position& pos);
