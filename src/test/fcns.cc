#include <iostream>

#define TEST(var) \
  std::cout << "\033[36m" #var "\033[0m = " << var << std::endl;

#define CHECK(var,val) { \
  const auto _var = var; \
  const auto _val = val; \
  std::cout << "\033[36m" #var "\033[0m = " << _var; \
  if (_var==_val) std::cout << " \033[32m✔\033[0m"; \
  else std::cout << " \033[31m" << _val << "\033[0m"; \
  std::cout << std::endl; \
}

#include <histograms/histograms.hh>

using std::cout;
using std::endl;

int main(int argc, char* argv[]) {

  histograms::histogram h({
    {0,1,2,3,4,5},
    {1,10,100}
  });

  CHECK(h.join_index(5), 5)
  CHECK(h.join_index(5,2), 5+2*7)
  CHECK(h.join_index({5,2}), 5+2*7)
  // CHECK(h.join_index({5},2), 5+2*7) // wrong

  CHECK(h.fill_at(19), 1)
  CHECK(h.fill_at({2,5},3.2), 3.2)

  CHECK(h.bin_at(19), 1)
  CHECK(h.bin_at(2,5), 3.2)

  CHECK(h.find_bin_index(4.5,15), 19)
  CHECK(h.find_bin(4.5,15), 1)

  CHECK(h.fill_at({5,2}), 2)

  CHECK(h.fill(4.5,15), 3)
  CHECK(h.fill({4.5,15.}), 4)
  CHECK(h.fill({4.5,15.},2.5), 6.5)
  CHECK(h.fill(std::forward_as_tuple(4.5,15),2.1), 8.6)

}
