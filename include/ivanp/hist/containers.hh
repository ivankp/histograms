#ifndef IVANP_CONTAINERS_HH
#define IVANP_CONTAINERS_HH

#include <tuple>
#include <array>
#include <vector>
#include <ivanp/hist/traits.hh>

namespace ivanp::hist {
namespace containers {

IVANP_MAKE_OP_TRAIT_2( is_indexable, std::declval<T&>()[std::declval<T2&>()] )
IVANP_MAKE_OP_TRAIT_2( has_at, std::declval<T&>().at(std::declval<T2&>()) )
IVANP_MAKE_OP_TRAIT_1( has_size, std::declval<T&>().size() )
IVANP_MAKE_OP_TRAIT_1( has_tuple_size, std::tuple_size<T>::value )
IVANP_MAKE_OP_TRAIT_2( is_resizable,
  std::declval<T&>().resize(std::declval<T2>()) )

template <typename T>
inline constexpr auto size(T&) noexcept -> decltype(std::tuple_size<T>::value)
{ return std::tuple_size<T>::value; }
template <typename T>
inline constexpr auto size(T& x) noexcept -> std::enable_if_t<
  !has_tuple_size<T>::value, std::size_t
> {
  return x.size();
}

template <typename T>
using has_either_size = std::disjunction<has_size<T>,has_tuple_size<T>>;

template <typename C, typename I>
inline auto at(C& c, I i) -> decltype(c[i]) { return c[i]; }
template <typename C, typename I>
inline auto at(C& c, I i) -> std::enable_if_t<
  !is_indexable<C,I>::value, decltype(c.at(i))
> { return c.at(i); }

namespace detail { namespace multiple_iteration {

using std::get;
using std::begin;
using std::end;
using std::declval;

template <typename T, size_t I, typename Enable = void>
struct iterator;

template <typename T, size_t I>
class iterator<T,I,
  std::enable_if_t< has_tuple_size<T>::value >
> { // tuple
  T& x;
  static constexpr auto size = std::tuple_size<T>::value;
public:
  iterator(T& x): x(x) { }
  decltype(auto) operator*() { return get<I>(x); }
  decltype(auto) operator++() { return iterator<T,I+1>(x); }
  constexpr bool operator!() const noexcept { return I >= size; }
  static constexpr bool is_known_last = (I+1 >= size);
  static constexpr bool is_tuple = true;
};

template <typename T, size_t I>
class iterator<T,I,
  std::enable_if_t< !has_tuple_size<T>::value >
> { // vector
  decltype(begin(declval<T&>())) it;
  decltype(end(declval<T&>())) _end;
public:
  iterator(T& x): it(begin(x)), _end(end(x)) { }
  decltype(auto) operator*() { return *it; }
  decltype(auto) operator++() { ++it; return *this; }
  bool operator!() noexcept { return it == _end; }
  static constexpr bool is_known_last = false;
  static constexpr bool is_tuple = false;
};

template <typename... It, typename F>
inline void for_each(F&& f, It&&... it) {
  if constexpr (!(std::remove_reference_t<It>::is_tuple && ...))
    if ((!it || ...)) return;
  f(*it...);
  if constexpr (!(std::remove_reference_t<It>::is_known_last || ...))
    detail::multiple_iteration::for_each( std::forward<F>(f), ++it... );
}

}}

template <typename... T, typename F>
inline void for_each(F&& f, T&&... xs) {
  detail::multiple_iteration::for_each(
    std::forward<F>(f),
    detail::multiple_iteration::iterator<std::remove_reference_t<T>,0>(xs)...
  );
}

template <typename T, template<typename> typename F>
struct transform;

template <typename T, typename Alloc, template<typename> typename F>
struct transform<std::vector<T,Alloc>,F> {
  using type = std::vector<typename F<T>::type>;
  using arg_type = std::initializer_list<typename F<T>::type>;
};

template <typename... T, template<typename> typename F>
struct transform<std::tuple<T...>,F> {
  using type = std::tuple<typename F<T>::type...>;
  using arg_type = type;
};

template <typename T, size_t N, template<typename> typename F>
struct transform<std::array<T,N>,F> {
  using type = std::array<typename F<T>::type,N>;
  using arg_type = type;
};

}}

#endif
