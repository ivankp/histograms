#ifndef IVANP_CONTAINERS_HH
#define IVANP_CONTAINERS_HH

#include <tuple>
#include <histograms/traits.hh>

namespace histograms {

IVANP_MAKE_OP_TRAIT_2( is_indexable, std::declval<T&>()[std::declval<T2&>()] )
IVANP_MAKE_OP_TRAIT_2( has_at, std::declval<T&>().at(std::declval<T2&>()) )
IVANP_MAKE_OP_TRAIT_1( has_size, std::declval<T&>().size() )
IVANP_MAKE_OP_TRAIT_1( has_tuple_size, std::tuple_size<T>::value )

namespace containers {

template <typename T>
inline constexpr auto size(T&) noexcept -> decltype(std::tuple_size<T>::value)
{ return std::tuple_size<T>::value; }
template <typename T>
inline constexpr auto size(T& x) noexcept -> std::enable_if_t<
  !has_tuple_size<T>::value, decltype(x.size())
> { return x.size(); }

IVANP_MAKE_OP_TRAIT_1( has_size, containers::size(std::declval<T&>()) )

template <typename C, typename I>
inline auto at(C& c, I i) -> decltype(c[i]) { return c[i]; }
template <typename C, typename I>
inline auto at(C& c, I i) -> std::enable_if_t<
  !is_indexable<C,I>::value, decltype(c.at(i))
> { return c.at(i); }

namespace detail { namespace for_each {

using std::get;
using std::begin;
using std::end;
using std::declval;

template <typename T, size_t I, typename Enable = void>
struct iterator;

// TODO: bug, doesn't get used for tuples
template <typename T, size_t I>
class iterator<T,I,
  std::enable_if_t< has_tuple_size<T>::value >
> { // tuple
  T& x;
  static constexpr auto size = std::tuple_size<T>::value;
public:
  iterator(T& x): x(x) { }
  constexpr decltype(auto) operator*() { return get<I>(x); }
  constexpr decltype(auto) operator++() { return iterator<T,I+1>(x); }
  constexpr bool operator!() const noexcept { return I >= size; }
  static constexpr bool is_known_last = (I+1 >= size);
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
};

template <typename... T, typename F>
void for_each(F&& f, T&&... it) {
  if ((!!it && ...)) {
    f(*it...);
    if constexpr (!(std::remove_reference_t<T>::is_known_last || ...))
      detail::for_each::for_each(std::forward<F>(f),++it...);
  }
}

}}

template <typename... T, typename F>
void for_each(F&& f, T&&... xs) {
  detail::for_each::for_each(
    std::forward<F>(f),
    detail::for_each::iterator<std::remove_reference_t<T>,0>(xs)...
  );
}

}}

#endif
