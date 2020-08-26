#ifndef IVANP_CONTAINERS_GENERAL_HH
#define IVANP_CONTAINERS_GENERAL_HH

#include <ivanp/cont/concepts.hh>

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
  else
    return *(std::begin(std::forward<C>(c)));
}
template <Container C>
[[nodiscard]]
inline decltype(auto) last(C&& c) {
  if constexpr (Tuple<C>)
    return std::get<std::tuple_size_v<C>-1>(std::forward<C>(c));
  else
    return *(std::end(std::forward<C>(c)));
}

template <Container C>
using first_type = std::decay_t<decltype(first(std::declval<C&>()))>;
template <Container C>
using last_type = std::decay_t<decltype(last(std::declval<C&>()))>;

}

#endif
