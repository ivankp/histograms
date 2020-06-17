#ifndef IVANP_HISTOGRAMS_BIN_FILLER_HH
#define IVANP_HISTOGRAMS_BIN_FILLER_HH

#include <ivanp/hist/traits.hh>

namespace ivanp::hist {

struct bin_filler {

  template <typename Bin>
  static std::enable_if_t<
    has_pre_increment<Bin>::value,
    Bin&
  >
  fill(Bin& bin) noexcept(noexcept(++bin)) { return ++bin; }

  template <typename Bin>
  static std::enable_if_t<
    !has_pre_increment<Bin>::value &&
    has_post_increment<Bin>::value,
    Bin&
  >
  fill(Bin& bin) noexcept(noexcept(bin++)) { return bin++; }

  template <typename Bin>
  static std::enable_if_t<
    !has_pre_increment<Bin>::value &&
    !has_post_increment<Bin>::value &&
    is_callable<Bin>::value,
    Bin&
  >
  fill(Bin& bin) noexcept(noexcept(bin())) { return bin(); }

  template <typename Bin, typename T>
  static std::enable_if_t<
    has_plus_eq<Bin,T>::value,
    Bin&
  >
  fill(Bin& bin, T&& x) noexcept(noexcept(bin += std::forward<T>(x)))
  { return bin += std::forward<T>(x); }

  template <typename Bin, typename T1, typename... TT>
  static std::enable_if_t<
    ( sizeof...(TT) || !has_plus_eq<Bin,T1>::value ) &&
    is_callable<Bin,T1,TT...>::value,
    Bin&
  >
  fill(Bin& bin, T1&& arg1, TT&&... args)
  noexcept(noexcept(bin(std::forward<T1>(arg1), std::forward<TT>(args)...)))
  { return bin(std::forward<T1>(arg1), std::forward<TT>(args)...); }

};

} // end namespace ivanp::hist

#endif
