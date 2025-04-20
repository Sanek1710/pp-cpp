#include <cstddef>
#include <string>
#include <type_traits>
#include <vector>

template <typename T>
inline size_t memory(const T& obj) {
  return sizeof(obj);
}

template <>
inline size_t memory(const std::string& str) {
  static const size_t sso_size = std::string{}.capacity();
  size_t total = sizeof(std::string);
  total += str.capacity() > sso_size ? str.capacity() : 0;
  return total;
}

template <typename T>
inline size_t memory(const std::vector<T>& vec) {
  size_t total = sizeof(std::vector<T>);
  total += vec.capacity() * sizeof(T);
  for (const T& v : vec) total += memory(v) - sizeof(v);
  return total;
}

template <typename T1, typename T2>
inline size_t memory(const std::pair<T1, T2>& p) {
  size_t total = sizeof(std::pair<T1, T2>);
  total += memory(p.first) - sizeof(p.first);
  total += memory(p.second) - sizeof(p.second);
  return total;
}

