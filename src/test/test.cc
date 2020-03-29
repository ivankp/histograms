#include <iostream>
#include <array>
#include <list>
#include <memory>

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

  histograms::histogram h1({
    {1,10,100,1000},
    {}, {1}, {1}, {1}
  });
  CHECK(h1.axes()[0].nbins(), 3)
  CHECK(h1.find_bin_index(20,5,0,0,0), 2)
  CHECK(h1.bins().size(), 1*2*2*2*5)

  for (const auto& bin : h1)
    cout << bin << ' ';
  cout << endl;

  cout << endl;
  histograms::histogram<
    double,
    std::array<histograms::uniform_axis<double>,2>
  > h2({{{9,1,10},{4,0,20}}});
  CHECK(h2.axes()[0].nedges(), 10)
  CHECK(h2.axes()[1].nedges(), 5)
  CHECK(h2.axes()[0].upper(0), 1)
  CHECK(h2.axes()[0].upper(1), 2)
  CHECK(h2.axes()[0].upper(2), 3)
  CHECK((h2.join_index({2,2})), 2+11*2)
  CHECK((h2.join_index(3,2)), 25)
  CHECK((h2.find_bin_index(std::vector<int>{2,16})), 2+11*4)
  CHECK(h2.axes()[0].find_bin_index(2), 2)
  CHECK(h2.axes()[1].find_bin_index(13), 3)
  CHECK(h2.bins().size(), 66)

  TEST(h2(std::tuple{4,7},3.14e8))
  TEST(h2.find_bin(4,7))
  TEST(h2.bin_at(4,2))
  TEST(h2.bin_at({4,2}))
  TEST(h2({5,10.5},5.5))

  cout << endl;
  // shared axis
  auto a3 = std::make_shared<histograms::container_axis<>>(
    std::vector<double>{10,20,30});
  histograms::histogram<
    int,
    std::tuple<
      histograms::uniform_axis<unsigned,true>,
      decltype(a3) >,
    histograms::bins_container_spec<std::list<int>>
  > h3({{4,1,5},a3});
  CHECK(h3.bins().size(), 24)

  cout << endl;
  [](const auto&... x){ TEST(__PRETTY_FUNCTION__) }(
    h3,
    h3.axes(),
    h3.bins(),
    std::get<1>(h3.axes())
  );

  TEST(std::get<1>(h3.axes()).use_count())

  histograms::histogram<
    double,
    std::list<histograms::uniform_axis<double>>
  > h4({{0,1,10}});
  CHECK(h4(0.4),1)
  CHECK(h4({0.4},1.5),2.5)
  CHECK(h4(0.4,1.5),3.5)
}
