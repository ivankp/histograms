#ifndef IVANP_CONTAINERS_GENERAL_HH
#define IVANP_CONTAINERS_GENERAL_HH

#include <ivanp/cont/concepts.hh>
#include <iterator>

namespace ivanp::cont {

template <typename C, typename I>
[[nodiscard, gnu::always_inline]]
inline decltype(auto) at(C&& xs, I i) {
  if constexpr (requires { xs[i]; })
    return xs[i];
  else if constexpr (requires { xs.at(i); })
    return xs.at(i);
}

template <Container C>
[[nodiscard, gnu::always_inline]]
constexpr auto size(const C& c) noexcept {
  if constexpr (Tuple<C>)
    return std::tuple_size<C>::value;
  else
    return std::size(c);
}

template <Container C>
[[nodiscard]]
inline decltype(auto) first(C&& c) {
  if constexpr (Tuple<C>)
    return std::get<0>(std::forward<C>(c));
  else if constexpr (requires { std::forward<C>(c).front(); })
    return std::forward<C>(c).front();
  else
    return *(std::begin(std::forward<C>(c)));
}
template <Container C>
[[nodiscard]]
inline decltype(auto) last(C&& c) {
  if constexpr (Tuple<C>)
    return std::get<std::tuple_size_v<C>-1>(std::forward<C>(c));
  else if constexpr (requires { std::forward<C>(c).back(); })
    return std::forward<C>(c).back();
  else if constexpr (requires { std::prev(std::end(std::forward<C>(c))); })
    return *std::prev(std::end(std::forward<C>(c)));
  else {
    const auto n = size(c);
    if (n==0) throw std::out_of_range(
      "cannot get last element of container with length zero");
    return *std::next(std::begin(std::forward<C>(c)),n-1);
  }
}

template <Container C>
using first_type = std::decay_t<decltype(first(std::declval<C&>()))>;
template <Container C>
using last_type = std::decay_t<decltype(last(std::declval<C&>()))>;

template <Iterable C>
auto forwarding_iterators(C&& c) noexcept {
  if constexpr (std::is_rvalue_reference_v<C&&>)
    return std::pair(
      std::move_iterator(std::begin(c)),
      std::move_iterator(std::end  (c))
    );
  else
    return std::pair( std::begin(c), std::end(c) );
}

}

#endif
