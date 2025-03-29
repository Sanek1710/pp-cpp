#pragma once

#include <iterator>
#include <type_traits>
#include <utility>

template <typename Iterator>
class Range {
 private:
  Iterator mbegin;
  Iterator mend;

  using deref_type = decltype(*mbegin);
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

  deref_type front() const { return *mbegin; }
  deref_type back() const { return *std::prev(mend); }
};

template <typename Iterator>
Range(Iterator, Iterator) -> Range<Iterator>;

template <typename Container>
Range(const Container&)
    -> Range<decltype(std::begin(std::declval<Container>()))>;
