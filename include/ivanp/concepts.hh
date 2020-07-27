#ifndef IVANP_CONCEPTS_HH
#define IVANP_CONCEPTS_HH

#include <type_traits>

namespace ivanp {

template <typename From, typename To>
concept convertible_to = std::is_convertible_v<From, To>;

template <typename From, typename To>
concept convertible_to_or_any = std::conditional_t<
  std::is_void_v<To>, // any type if To == void
  std::true_type,
  std::is_convertible<From, To>
>::value;

template <typename T>
concept stringlike = requires (T& x) {
  { std::data(x) } -> convertible_to<const char*>;
  { std::size(x) } -> convertible_to<size_t>;
};

template <typename A, typename... B>
concept either = std::disjunction_v<std::is_same<A,B>...>;

}

#endif
