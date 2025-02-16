#include <cstddef>
#include <string>
#include <type_traits>
#include <vector>

// Forward declaration - but only for non-integral types
template <typename T, typename = std::enable_if_t<!std::is_integral_v<std::remove_cv_t<std::remove_reference_t<T>>>>>
inline size_t memory(const T& t);

// Integral types specialization
template <typename T>
inline std::enable_if_t<std::is_integral_v<std::remove_cv_t<std::remove_reference_t<T>>>, size_t>
memory(const T& i) {
  return sizeof(i);
}

template <>
inline size_t memory(const std::string& str) {
  return sizeof(std::string) + (str.capacity() > 16 ? str.capacity() : 0);
}

template <typename T>
inline size_t memory(const std::vector<T>& vec) {
  size_t nbytes = sizeof(std::vector<T>);
  nbytes += (vec.capacity() - vec.size()) * sizeof(T);
  for (const T& v : vec) {
    nbytes += memory(v);
  }
  return nbytes;
}

template <typename T1, typename T2>
inline size_t memory(const std::pair<T1, T2>& p) {
  size_t nbytes = sizeof(std::pair<T1, T2>) - sizeof(T1) - sizeof(T2);
  nbytes += memory(p.first);
  nbytes += memory(p.second);
  return nbytes;
}
