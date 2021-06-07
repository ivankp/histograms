#ifndef IVANP_MAP_TRAITS_HH
#define IVANP_MAP_TRAITS_HH

#include <type_traits>

namespace ivanp {

template <typename...> using void_t = void;

template <typename T, typename...> struct pack_head { using type = T; };
template <typename... T> using head_t = typename pack_head<T...>::type;

template <typename T>
struct type_constant { using type = T; };

template <typename...>
struct type_sequence { };

template <size_t I>
using index_constant = std::integral_constant<size_t,I>;

template <typename...>
struct are_same: std::true_type { };
template <typename A, typename... B>
struct are_same<A,B...>: std::bool_constant<(... && std::is_same_v<A,B>)> { };
template <typename... T>
constexpr bool are_same_v = are_same<T...>::value;

} // end namespace ivanp

#endif
