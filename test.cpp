
#include <iostream>
#include <vector>

#include "helper.h"
#include "RangeView.h"

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

int main(int argc, char *argv[]) {
  std::cout << "tests"
            << "\n";
  return 0;
}