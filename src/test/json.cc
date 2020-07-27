#include <iostream>
#include <map>

#include <ivanp/hist/histograms.hh>
#include <ivanp/hist/bins.hh>
#include <ivanp/hist/json.hh>

#define STR1(x) #x
#define STR(x) STR1(x)

#define TEST(var) std::cout << \
  "\033[33m" STR(__LINE__) ": " \
  "\033[36m" #var ":\033[0m " << (var) << std::endl;

using std::cout;
using std::endl;
using namespace std::string_literals;

// using json = nlohmann::json;
// using namespace ivanp::hist;

int main(int argc, char* argv[]) {
  ivanp::hist::histogram<ivanp::hist::mc_bin<>> h({{1,2,3}});
  std::map hs {
    std::pair{"h1"s, &h},
    {"h2"s, &h}
  };

  // for (auto x : h)
  //   cout << ' ' << x;
  // cout << endl;

  // cout << ivanp::hist::to_json(hs) << endl;
  cout << nlohmann::json(hs) << endl;
  // cout << ivanp::hist::merge_defs(json{hs}) << endl;
}
