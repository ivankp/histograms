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

template <typename T>
using hist = histograms::histogram<T>;

int main(int argc, char* argv[]) {

  hist<hist<double>> h({{0,1}});

}
