#ifndef IVANP_CONCEPTS_HH
#define IVANP_CONCEPTS_HH

#include <type_traits>

namespace ivanp {

template <typename From, typename To>
concept convertible_to = std::is_convertible_v<From, To>;

template <typename T>
concept stringlike = requires (T& x) {
  { std::data(x) } -> convertible_to<const char*>;
  { std::size(x) } -> convertible_to<size_t>;
};

template <typename A, typename... B>
concept either = std::disjunction_v<std::is_same<A,B>...>;

}

#endif
