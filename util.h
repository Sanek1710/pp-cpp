#pragma once

#include "ankerl/unordered_dense.h"

namespace dense = ankerl::unordered_dense;

struct string_hash {
  using is_transparent = void;  // enable heterogeneous overloads
  using is_avalanching = void;  // mark class as high quality avalanching hash

  [[nodiscard]] auto operator()(std::string_view str) const noexcept
      -> uint64_t {
    return dense::hash<std::string_view>{}(str);
  }
};

template <typename Value>
using StringMap = dense::map<std::string, Value, string_hash, std::equal_to<>>;
using StringSet = dense::set<std::string, string_hash, std::equal_to<>>;
