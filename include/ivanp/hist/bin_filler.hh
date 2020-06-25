#ifndef IVANP_HISTOGRAMS_BIN_FILLER_HH
#define IVANP_HISTOGRAMS_BIN_FILLER_HH

#include <type_traits>
#include <functional>

namespace ivanp::hist {

template <typename T>
concept can_pre_increment = requires (T& x) { ++x; };
template <typename T>
concept can_post_increment = requires (T& x) { x++; };
template <typename T, typename Arg>
concept can_increment_by = requires (T& x, Arg arg) { x += arg; };
template <typename F, typename... Args>
concept can_invoke = std::is_invocable_v<F,Args...>;

struct bin_filler {

  template <typename Bin>
  requires can_pre_increment<Bin>
  static decltype(auto) fill(Bin& bin)
  noexcept(noexcept(++bin))
  { return ++bin; }

  template <typename Bin>
  requires (!can_pre_increment<Bin>) && can_post_increment<Bin>
  static decltype(auto) fill(Bin& bin)
  noexcept(noexcept(bin++))
  { return bin++; }

  template <typename Bin>
  requires (!can_pre_increment<Bin>) && (!can_post_increment<Bin>)
        && can_invoke<Bin&>
  static decltype(auto) fill(Bin& bin)
  noexcept(std::is_nothrow_invocable_v<Bin&>)
  { return std::invoke(bin); }

  template <typename Bin, typename T>
  requires can_increment_by<Bin,T&&>
  static decltype(auto) fill(Bin& bin, T&& x)
  noexcept(noexcept(bin += std::forward<T>(x)))
  { return bin += std::forward<T>(x); }

  template <typename Bin, typename T1, typename... TT>
  requires ( sizeof...(TT) || !can_increment_by<Bin,T1&&>)
        && can_invoke<Bin&,T1&&,TT&&...>
  static decltype(auto) fill(Bin& bin, T1&& x1, TT&&... xs)
  noexcept(std::is_nothrow_invocable_v<Bin&,T1&&,TT&&...>)
  { return std::invoke(bin, std::forward<T1>(x1), std::forward<TT>(xs)...); }

};

} // end namespace ivanp::hist

#endif
