#include <histograms/containers.hh>
#include <iostream>
#include <vector>
#include <list>

#define TEST(var) \
  std::cout << "\033[36m" #var "\033[0m = " << var << std::endl;

using histograms::containers::for_each;
using std::cout;
using std::endl;

int main(int argc, char* argv[]) {

  for_each([](auto a, auto b){
    cout << a <<' '<< b << endl;
  }, std::vector{1,2,3}, std::list{'a','b','c'});

  const std::tuple t2{'a','b','c'};
  for_each([](auto a, auto b){
    cout << a <<' '<< b << endl;
  }, std::tuple{1,2,3}, t2);

  for_each([](auto a, auto b){
    cout << a <<' '<< b << endl;
  }, std::vector{1,2,3}, std::tuple{'a','b','c'});
}
