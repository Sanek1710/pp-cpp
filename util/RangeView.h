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
 public:
  using iterator = Iterator;
  using value_type = typename std::iterator_traits<Iterator>::value_type;
  using difference_type =
      typename std::iterator_traits<Iterator>::difference_type;
  using reference = typename std::iterator_traits<Iterator>::reference;
  using size_type = size_t;

  Range(Iterator begin, Iterator end) : mbegin(begin), mend(end) {}

  template <typename Container>
  Range(const Container& container)
      : mbegin(std::begin(container)), mend(std::end(container)) {}

  iterator begin() const { return mbegin; }
  iterator end() const { return mend; }

  size_type size() const { return std::distance(mbegin, mend); }
  bool empty() const { return mbegin == mend; }

  reference front() const { return *mbegin; }
  reference back() const { return *std::prev(mend); }

  reference operator[](difference_type n) const { return *(mbegin + n); }

  void remove_prefix(size_type n) { mbegin += n; }
  void remove_suffix(size_type n) { mend -= n; }
  Range slice(difference_type from, difference_type to) const {
    return Range(mbegin + from, mend + to);
  }

 private:
  Iterator mbegin;
  Iterator mend;
};

template <typename Iterator>
Range(Iterator, Iterator) -> Range<Iterator>;

template <typename Container>
Range(const Container&)
    -> Range<decltype(std::begin(std::declval<Container>()))>;

template <typename Container>
class IndexIterator {
 public:
  using iterator_category = std::random_access_iterator_tag;
  using value_type = typename Container::value_type;
  using difference_type = std::ptrdiff_t;
  using pointer = value_type*;
  using reference = value_type&;

  IndexIterator(Container& cont, size_t idx) : container(&cont), mindex(idx) {}

  reference operator*() { return (*container)[mindex]; }
  pointer operator->() { return &(*container)[mindex]; }

  IndexIterator& operator++() {
    ++mindex;
    return *this;
  }
  IndexIterator operator++(int) {
    auto tmp = *this;
    ++mindex;
    return tmp;
  }
  IndexIterator& operator--() {
    --mindex;
    return *this;
  }
  IndexIterator operator--(int) {
    auto tmp = *this;
    --mindex;
    return tmp;
  }

  IndexIterator& operator+=(difference_type n) {
    mindex += n;
    return *this;
  }
  IndexIterator operator+(difference_type n) const {
    return IndexIterator(*container, mindex + n);
  }
  IndexIterator& operator-=(difference_type n) {
    mindex -= n;
    return *this;
  }
  IndexIterator operator-(difference_type n) const {
    return IndexIterator(*container, mindex - n);
  }
  difference_type operator-(const IndexIterator& other) const {
    return mindex - other.mindex;
  }

  bool operator==(const IndexIterator& other) const {
    return mindex == other.mindex;
  }
  bool operator!=(const IndexIterator& other) const {
    return !(*this == other);
  }
  bool operator<(const IndexIterator& other) const {
    return mindex < other.mindex;
  }
  bool operator>(const IndexIterator& other) const { return other < *this; }
  bool operator<=(const IndexIterator& other) const { return !(other < *this); }
  bool operator>=(const IndexIterator& other) const { return !(*this < other); }

  size_t index() const { return mindex; }

 private:
  Container* container;
  size_t mindex;
};

template <typename Container>
class IndexRange {
 public:
  using iterator = IndexIterator<Container>;
  using value_type = typename Container::value_type;
  using difference_type = std::ptrdiff_t;
  using reference = value_type&;
  using size_type = size_t;

  IndexRange(Container& cont)
      : container(&cont), mibegin(0), miend(std::size(cont)) {}
  IndexRange(Container& cont, size_t begin)
      : container(&cont), mibegin(begin), miend(std::size(cont)) {}
  IndexRange(Container& cont, size_t begin, size_t end)
      : container(&cont), mibegin(begin), miend(end) {}

  iterator begin() const { return iterator(*container, mibegin); }
  iterator end() const { return iterator(*container, miend); }

  size_t size() const { return miend - mibegin; }
  bool empty() const { return mibegin == miend; }

  reference front() const { return (*container)[mibegin]; }
  reference back() const { return (*container)[miend - 1]; }

  reference operator[](size_t n) const { return (*container)[mibegin + n]; }

  void remove_prefix(size_type n) { mibegin += n; }
  void remove_suffix(size_type n) { miend -= n; }
  IndexRange slice(size_t from, size_t to) const {
    return IndexRange(*container, mibegin + from, mibegin + to);
  }

  size_t ibegin() const { return mibegin; }
  size_t iend() const { return miend; }

 private:
  Container* container;
  size_t mibegin;
  size_t miend;
};
