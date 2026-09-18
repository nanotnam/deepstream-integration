#include "deepstream_platform/runtime.hpp"

#include <iostream>

int main() {
  std::cout << deepstream_platform::boundary_name() << " application template\n";
  return 0;
}
