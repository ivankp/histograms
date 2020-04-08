#ifndef IVANP_HISTOGRAMS_HH
#define IVANP_HISTOGRAMS_HH

#include <type_traits>
#include <tuple>
#include <array>
#include <vector>
#include <string>

#include <histograms/axes.hh>
#include <histograms/bin_filler.hh>
#include <histograms/containers.hh>

namespace histograms {

template <typename> struct bins_container_spec { };
template <typename> struct is_bins_container_spec {
  static constexpr bool value = false;
};
template <typename T>
struct is_bins_container_spec<bins_container_spec<T>> {
  static constexpr bool value = true;
  using type = T;
};

template <typename> struct filler_spec { };
template <typename> struct is_filler_spec {
  static constexpr bool value = false;
};
template <typename T>
struct is_filler_spec<filler_spec<T>> {
  static constexpr bool value = true;
  using type = T;
};

namespace detail {

template <typename T>
[[nodiscard, gnu::const, gnu::always_inline]]
inline auto axis_ref(T& x) -> decltype(x.nedges(),x)
{ return x; }

template <typename T, decltype(std::declval<T&>()->nedges(),int{}) = 0>
[[nodiscard, gnu::const, gnu::always_inline]]
inline decltype(auto) axis_ref(T& x)
{ return axis_ref(*x); }

template <typename Axis>
struct edge_type_from_axis {
  using type = typename std::decay_t<
    decltype(axis_ref(std::declval<Axis&>()))
  >::edge_type;
};

template <typename Axes>
using coord_arg_t = typename containers::transform<
  Axes, detail::edge_type_from_axis >::arg_type;

IVANP_MAKE_OP_TRAIT_1( has_coord_arg, std::declval<coord_arg_t<T>&>() )

}

template <
  typename Bin = double,
  typename Axes = std::vector<container_axis<std::vector<double>>>,
  typename... Specs
>
class histogram {
public:
  using bin_type = Bin;
  using axes_type = Axes;
  using bins_container_type = typename std::disjunction<
    is_bins_container_spec<Specs>...,
    is_bins_container_spec<bins_container_spec< std::vector<bin_type> >>
  >::type;
  using filler_type = typename std::disjunction<
    is_filler_spec<Specs>...,
    is_filler_spec<filler_spec< bin_filler >>
  >::type;
  using index_type = histograms::index_type;

private:
  axes_type _axes;
  bins_container_type _bins;

  void resize_bins() {
    if constexpr (
      containers::is_resizable<bins_container_type,size_t>::value
    ) {
      if (containers::size(_axes) > 0) {
        index_type n = 1;
        containers::for_each([&n](const auto& a){
          n *= detail::axis_ref(a).nedges()+1;
        }, _axes);
        _bins.resize(n);
      }
    }
  }

public:
  histogram() { }
  histogram(const histogram&) = default;
  histogram(histogram&&) = default;
  histogram& operator=(const histogram&) = default;
  histogram& operator=(histogram&&) = default;
  // ~histogram() = default; // TODO: do I need to do this?

  histogram(const axes_type& axes): _axes(axes) { resize_bins(); }
  histogram(axes_type&& axes)
  noexcept(std::is_nothrow_move_constructible_v<axes_type>)
  : _axes(std::move(axes)) { resize_bins(); }

  const axes_type& axes() const noexcept { return _axes; }
  const bins_container_type& bins() const noexcept { return _bins; }
  bins_container_type& bins() noexcept { return _bins; }
  auto size() const noexcept { return containers::size(_bins); }

  auto begin() const noexcept { return _bins.begin(); }
  auto begin() noexcept { return _bins.begin(); }
  auto   end() const noexcept { return _bins.end(); }
  auto   end() noexcept { return _bins.end(); }

  // ----------------------------------------------------------------

  const bin_type& bin_at(index_type i) const {
    return containers::at(_bins,i);
  }
  bin_type& bin_at(index_type i) {
    return containers::at(_bins,i);
  }

  index_type join_index(index_type i) const noexcept { return i; }

  template <typename T = std::initializer_list<index_type>>
  auto join_index(const T& ii) const -> std::enable_if_t<
    containers::has_either_size<T>::value &&
    !std::is_convertible<T,index_type>::value,
    index_type
  > {
    index_type index = 0, n = 1;
    containers::for_each([&](index_type i, const auto& a){
      index += i*n;
      n *= detail::axis_ref(a).nedges()+1;
    },ii,_axes);
    return index;
  }

  template <typename... T>
  auto join_index(const T&... ii) const
  -> std::enable_if_t< (sizeof...(ii) > 1), index_type > {
    // array is faster here because of for_each
    return join_index(std::array<index_type,sizeof...(ii)>{index_type(ii)...});
  }

  index_type join_index() const { return 0; }

  const bin_type& bin_at(std::initializer_list<index_type> ii) const {
    return bin_at(join_index(ii));
  }
  bin_type& bin_at(std::initializer_list<index_type> ii) {
    return bin_at(join_index(ii));
  }
  template <typename... T>
  const bin_type& bin_at(const T&... ii) const {
    return bin_at(join_index(ii...));
  }
  template <typename... T>
  bin_type& bin_at(const T&... ii) {
    return bin_at(join_index(ii...));
  }

  template <typename T = std::initializer_list<index_type>>
  const bin_type& operator[](const T& ii) const { return bin_at(ii); }
  template <typename T = std::initializer_list<index_type>>
  bin_type& operator[](const T& ii) { return bin_at(ii); }

  // ----------------------------------------------------------------

  template <typename T>
  auto find_bin_index(const T& xs) const -> std::enable_if_t<
    containers::has_either_size<const T>::value, index_type
  > {
    index_type index = 0, n = 1;
    containers::for_each([&](const auto& x, const auto& a){
      index += detail::axis_ref(a).find_bin_index(x)*n;
      n *= detail::axis_ref(a).nedges()+1;
    },xs,_axes);
    return index;
  }

  template <typename... T>
  auto find_bin_index(const T&... xs) const -> std::enable_if_t<
    (sizeof...(T) > 1)
    || !containers::has_either_size<const head_t<T...>>::value,
    index_type
  > {
    // if constexpr (containers::has_tuple_size<axes_type>::value)
    //   static_assert(std::tuple_size<axes_type>::value == sizeof...(xs));
    return find_bin_index(std::forward_as_tuple(xs...));
  }

  index_type find_bin_index() const { return 0; }

  template <typename... T>
  const bin_type& find_bin(const T&... xs) const {
    return bin_at(find_bin_index(xs...));
  }
  template <typename... T>
  bin_type& find_bin(const T&... xs) {
    return bin_at(find_bin_index(xs...));
  }

  // ----------------------------------------------------------------

  template <typename I = std::initializer_list<index_type>, typename... A>
  decltype(auto) fill_at(const I& ii, A&&... as) {
    return filler_type::fill(bin_at(ii),std::forward<A>(as)...);
  }

  // https://stackoverflow.com/q/60909588/2640636

  decltype(auto) fill() { return filler_type::fill(find_bin()); }

  template <typename X, typename... A>
  decltype(auto) fill(const X& xs, A&&... as) {
    if constexpr (containers::has_either_size<const X>::value) {
      return filler_type::fill(find_bin(xs),std::forward<A>(as)...);
    } else {
      return filler_type::fill(find_bin(xs,as...));
    }
  }
  template <typename... T>
  decltype(auto) operator()(const T&... xs) { return fill(xs...); }

  template <typename X, typename... A,
    typename = std::enable_if_t<
      !detail::has_coord_arg<head_t<Axes,X>>::value
    >
  >
  decltype(auto) fill(std::initializer_list<X> xs, A&&... as) {
    return filler_type::fill(find_bin(xs),std::forward<A>(as)...);
  }
  template <typename X, typename... A,
    typename = std::enable_if_t<
      !detail::has_coord_arg<head_t<Axes,X>>::value
    >
  >
  decltype(auto) operator()(std::initializer_list<X> xs, A&&... as) {
    return fill(xs,std::forward<A>(as)...);
  }

  template <typename... A, typename X = detail::coord_arg_t<head_t<Axes,A...>>>
  decltype(auto) fill(const head_t<X>& xs, A&&... as) {
    return filler_type::fill(find_bin(xs),std::forward<A>(as)...);
  }
  template <typename... A, typename X = detail::coord_arg_t<head_t<Axes,A...>>>
  decltype(auto) operator()(const head_t<X>& xs, A&&... as) {
    return fill(xs,std::forward<A>(as)...);
  }

};

} // end namespace histograms

#endif
