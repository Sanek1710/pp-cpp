#pragma once

#include <cassert>
#include <utility>
#include <vector>

template <typename T>
class VectorTail {
 public:
  using reference = typename std::vector<T>::reference;
  using size_type = typename std::vector<T>::size_type;
  using iterator = typename std::vector<T>::iterator;

  VectorTail(std::vector<T>& base) : mbase(&base), mhead(base.size()) {}
  VectorTail(std::vector<T>& base, size_type head)
      : mbase(&base), mhead(head) {}

  void push_back(const T& value) { mbase->push_back(value); }
  void push_back(T&& value) { mbase->push_back(std::move(value)); }
  template <typename... Args>
  void emplace_back(Args&&... args) {
    mbase->emplace_back(std::forward<Args>(args)...);
  }

  void append(VectorTail other) {
    const size_t other_size = other.size();
    for (size_t i = 0; i != other_size; ++i) {
      push_back(other[i]);
    }
  }
  void append(VectorTail other, size_type n) {
    for (size_t i = 0; i != n; ++i) {
      push_back(other[i]);
    }
  }

  iterator begin() { return mbase->begin() + mhead; }
  iterator end() { return mbase->end(); }

  size_type size() const { return mbase->size() - mhead; }
  bool empty() const { return mbase->size() == mhead; }

  reference front() const { return (*mbase)[mhead]; }
  reference back() const { return mbase->back(); }

  reference operator[](size_type n) { return (*mbase)[mhead + n]; }

  void remove_prefix(size_type n) { mhead += n; }
  void clear() { mbase->erase(begin(), end()); }

  VectorTail slice() const { return VectorTail(*mbase); }
  VectorTail slice(size_type from) const {
    return VectorTail(*mbase, mhead + from);
  }

  std::vector<T>& base() { return *mbase; }
  size_type head() const { return mhead; }

 private:
  std::vector<T>* mbase;
  size_type mhead;
};
