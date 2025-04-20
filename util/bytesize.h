#pragma once

#include <ostream>

struct ByteSize {
  ByteSize(size_t nbytes) : size(nbytes) {}
  size_t size;
};

inline std::ostream& operator<<(std::ostream& os, ByteSize bsize) {
  static constexpr size_t b = 1;
  static constexpr size_t Kb = b << 10U;
  static constexpr size_t Mb = Kb << 10U;
  static constexpr size_t Gb = Mb << 10U;

  static const char* suffixes[]{"b", "Kb", "Mb", "Gb"};

  size_t i = 0;
  if (bsize.size >= Gb)
    i = 3;
  else if (bsize.size >= Mb)
    i = 2;
  else if (bsize.size >= Kb)
    i = 1;
  else
    return os << bsize.size << " b";

  size_t size_m1 = bsize.size >> ((i - 1) * 10);
  return os << (size_m1 >> 10) << "."             //
            << (10 * (size_m1 % Kb) / Kb) << " "  //
            << suffixes[i];
}
