#include <iostream>
#include <array>
#include <list>

#define TEST(var) \
  std::cout << "\033[36m" #var "\033[0m = " << var << std::endl;

#include <histograms/histograms.hh>

using std::cout;
using std::endl;

int main(int argc, char* argv[]) {

  histograms::histogram h1({
    {1,10,100,1000},
    {1}, {1}, {1}, {1}
  });
  TEST(h1.axes()[0].nbins()); // 3
  TEST(h1.find_bin_index(20,0,0,0,0)) // 2

  for (const auto& bin : h1)
    cout << bin << ' ';
  cout << endl;

  cout << endl;
  histograms::histogram<
    double,
    std::array<histograms::uniform_axis<double>,2>
  > h2({{{9,1,10},{4,0,20}}});
  TEST(h2.axes()[0].nedges()) // 10
  TEST(h2.axes()[1].nedges()) // 5
  TEST(h2.axes()[0].upper(0)) // 1
  TEST(h2.axes()[0].upper(1)) // 2
  TEST(h2.axes()[0].upper(2)) // 3
  TEST(h2.join_index({2,2})) // 24
  TEST(h2.join_index(3,2)) // 25
  TEST(h2.find_bin_index(std::vector<int>{2,16})) // 46
  TEST(h2.axes()[0].find_bin_index(2)) // 2
  TEST(h2.axes()[1].find_bin_index(13)) // 3
  TEST(h2.bins().size()) // 66

  TEST(h2(std::tuple{4,7},3.14e8))
  TEST(h2.find_bin(4,7))
  TEST(h2.bin_at(4,2))
  TEST(h2.bin_at({4,2}))

  cout << endl;
  histograms::histogram<
    int,
    std::tuple<
      histograms::uniform_axis<unsigned,true>,
      histograms::container_axis<> >,
    histograms::bins_container_spec<std::list<int>>
  > h3({{4,1,5},{10,20,30}});
  TEST(h3.bins().size()) // 24

  cout << endl;
  [](const auto&... x){ TEST(__PRETTY_FUNCTION__) }(
    h3,
    h3.axes(),
    h3.bins()
  );
}
