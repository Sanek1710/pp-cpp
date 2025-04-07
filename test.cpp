
#include <iostream>
#include <vector>

#include "util/helper.h"
#include "util/RangeView.h"

testit(range) {
  std::vector<int> vec = {1, 2, 3, 4, 5};
  Range range1(vec);  // CTAD deduces Range<std::vector<int>::iterator>
  for (auto x : range1) {
    std::cout << x << " ";
  }
  std::cout << "\n";

  // From iterator pair
  Range range2(vec.begin() + 1, vec.end() - 1);  // CTAD works here
  for (auto x : range2) {
    std::cout << x << " ";
  }
  std::cout << "\n";

  // From array
  std::array<double, 3> arr = {1.1, 2.2, 3.3};
  Range range3(arr);  // CTAD deduces Range<std::array<double, 3>::iterator>
  for (auto x : range3) {
    std::cout << x << " ";
  }
  std::cout << "\n";

  // Test size and empty
  std::cout << "range1 size: " << range1.size() << "\n";
  std::cout << "range2 size: " << range2.size() << "\n";
  std::cout << "range1 empty: " << std::boolalpha << range1.empty() << "\n";
}

testit(slice) {
  std::vector<int> vec = {1, 2, 3, 4, 5};
  IndexRange slice1(vec);  // CTAD deduces Slice<std::vector<int>::iterator>
  for (auto x : slice1) {
    std::cout << x << " ";
  }
  std::cout << "\n";

  // From iterator pair
  IndexRange slice2(vec, 1, vec.size() - 1);  // CTAD works here
  for (auto x : slice2) {
    std::cout << x << " ";
  }
  std::cout << "\n";

  // From array
  std::array<double, 3> arr = {1.1, 2.2, 3.3};
  IndexRange slice3(arr);  // CTAD deduces Slice<std::array<double, 3>::iterator>
  for (auto x : slice3) {
    std::cout << x << " ";
  }
  std::cout << "\n";

  // Test size and empty
  std::cout << "slice1 size: " << slice1.size() << "\n";
  std::cout << "slice2 size: " << slice2.size() << "\n";
  std::cout << "slice1 empty: " << std::boolalpha << slice1.empty() << "\n";
}

int main(int argc, char *argv[]) {
  std::cout << "tests"
            << "\n";
  return 0;
}