#pragma once

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

template <typename Iterator>
class Range {
 private:
  Iterator mbegin;
  Iterator mend;

  using reference = decltype(*mbegin);
  using size_type = decltype(std::distance(mbegin, mend));

 public:
  Range(Iterator begin, Iterator end) : mbegin(begin), mend(end) {}

  template <typename Container>
  Range(const Container& container)
      : mbegin(std::begin(container)), mend(std::end(container)) {}

  Iterator begin() const { return mbegin; }
  Iterator end() const { return mend; }

  size_type size() const { return std::distance(mbegin, mend); }
  bool empty() const { return mbegin == mend; }

  reference front() const { return *mbegin; }
  reference back() const { return *std::prev(mend); }
};

template <typename Iterator>
Range(Iterator, Iterator) -> Range<Iterator>;

template <typename Container>
Range(const Container&)
    -> Range<decltype(std::begin(std::declval<Container>()))>;


template <typename Container>
class SliceIterator {
  using const_reference = typename Container::const_reference;
  using const_pointer = typename Container::const_pointer;

 public:
  SliceIterator(const Container& container, size_t i)
      : container(container), idx(i) {}

  constexpr const_reference operator*() const { return container[idx]; }
  constexpr const_pointer operator->() const { return &container[idx]; }
  constexpr SliceIterator& operator++() {
    ++idx;
    return *this;
  }
  constexpr SliceIterator operator++(int) {
    return SliceIterator{container, idx++};
  }

  bool operator==(SliceIterator other) const {
    return &this->container == &other.container && idx == other.idx;
  }
  bool operator!=(SliceIterator other) const { return !(*this == other); }

 private:
  const Container& container;
  size_t idx;
};

template <typename Container>
class Slice {
 private:
  const Container& container;
  size_t ibegin;
  size_t iend;

 public:
  using iterator = SliceIterator<Container>;
  using reference = decltype(std::declval<Container>()[std::declval<size_t>()]);

  Slice(const Container& container, size_t ibegin, size_t iend)
      : container(container),
        ibegin(std::clamp(ibegin, 0UL, std::size(container))),
        iend(std::clamp(iend, ibegin, std::size(container))) {}

  Slice(const Container& container, size_t ibegin)
      : container(container),
        ibegin(std::clamp(ibegin, 0UL, std::size(container))),
        iend(std::size(container)) {}

  Slice(const Container& container)
      : container(container), ibegin(0), iend(std::size(container)) {}

  iterator begin() const { return {container, ibegin}; }
  iterator end() const { return {container, iend}; }

  size_t size() const { return iend - ibegin; }
  bool empty() const { return ibegin == iend; }

  reference front() const { return container[ibegin]; }
  reference back() const { return container[iend - 1]; }
};
