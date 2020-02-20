#include <iostream>
#include <histograms/histograms.hh>

#define TEST(var) \
  std::cout << "\033[36m" #var "\033[0m = " << var << std::endl;

int main(int argc, char* argv[]) {

  histograms::histogram h1({{1,10,100,1000}});
  TEST(std::get<0>(h1.axes()).nbins());
  // TEST(h1.find_bin_index(20))

  histograms::histogram<
    double,
    std::vector<histograms::uniform_axis<double>>
  > h2({{10,1,100},{4,0,20}});
  TEST(h2.join_index({2,2}))
  // TEST(h2.find_bin_index(6,6))
}
