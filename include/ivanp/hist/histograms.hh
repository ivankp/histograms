#ifndef IVANP_HISTOGRAMS_HH
#define IVANP_HISTOGRAMS_HH

#include <type_traits>
#include <tuple>
#include <array>
#include <vector>
#include <string>

#include <ivanp/hist/axes.hh>
#include <ivanp/hist/bin_filler.hh>
#include <ivanp/map/map.hh>

namespace ivanp::hist {

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

template <typename Axis>
requires requires (Axis& a) { a.nedges(); }
[[nodiscard, gnu::const, gnu::always_inline]]
inline auto& axis_ref(Axis& a) { return a; }

template <typename Axis>
requires requires (Axis& a) { a->nedges(); }
[[nodiscard, gnu::const, gnu::always_inline]]
inline auto& axis_ref(Axis& x) { return axis_ref(*x); }

template <typename C, typename I>
[[nodiscard, gnu::always_inline]]
inline decltype(auto) at(C&& xs, I i) {
  if constexpr (requires { xs[i]; }) return xs[i];
  else if constexpr (requires { xs.at(i); }) return xs.at(i);
}

template <typename Axis>
using axis_edge_type = typename std::remove_reference_t<
    decltype(axis_ref(std::declval<Axis&>()))
  >::edge_type;

template <typename Axes>
struct coord_arg { };

template <typename Axes>
requires requires { typename Axes::value_type; }
struct coord_arg<Axes> {
  using type = std::initializer_list<
    axis_edge_type<typename Axes::value_type> >;
};

template <typename... Axes>
struct coord_arg<std::tuple<Axes...>> {
  using type = std::tuple<axis_edge_type<Axes>...>;
};

template <typename Axes, size_t N>
struct coord_arg<std::array<Axes,N>> {
  using type = std::array<axis_edge_type<Axes>,N>;
};

template <typename Axes>
using coord_arg_t = typename coord_arg<Axes>::type;

template <typename Axes>
concept ValidCoordArg = requires {
  typename coord_arg<Axes>::type;
};

}

template <
  typename Bin = double,
  typename Axes = std::vector<list_axis<std::vector<double>>>,
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
  using index_type = hist::index_type;

private:
  axes_type _axes;
  bins_container_type _bins;

  void resize_bins() {
    if constexpr (
      requires (index_type n) { _bins.resize(n); }
    ) {
      index_type n = 1;
      map::map([&n](const auto& a) {
        n *= detail::axis_ref(a).nedges()+1;
      }, _axes);
      _bins.resize(n);
    }
  }

public:
  histogram() = default;
  histogram(const histogram&) = default;
  histogram(histogram&&) = default;
  histogram& operator=(const histogram&) = default;
  histogram& operator=(histogram&&) = default;
  ~histogram() = default;

  histogram(const axes_type& axes): _axes(axes) { resize_bins(); }
  histogram(axes_type&& axes)
  noexcept(std::is_nothrow_move_constructible_v<axes_type>)
  : _axes(std::move(axes)) { resize_bins(); }

  const axes_type& axes() const noexcept { return _axes; }
  const bins_container_type& bins() const noexcept { return _bins; }
  bins_container_type& bins() noexcept { return _bins; }
  auto size() const noexcept {
    if constexpr (map::Tuple<bins_container_type>)
      return std::tuple_size<bins_container_type>::value;
    else
      return std::size(_bins);
  }
  auto naxes() const noexcept {
    if constexpr (map::Tuple<axes_type>)
      return std::tuple_size<axes_type>::value;
    else
      return std::size(_axes);
  }

  auto begin() const noexcept { return _bins.begin(); }
  auto begin() noexcept { return _bins.begin(); }
  auto   end() const noexcept { return _bins.end(); }
  auto   end() noexcept { return _bins.end(); }

  // ----------------------------------------------------------------

  index_type join_index(index_type i) const { return i; }

  template <typename T = std::initializer_list<index_type>>
  requires map::Container<T>
  index_type join_index(const T& ii) const {
    index_type index = 0, n = 1;
    map::map([&](index_type i, const auto& a) {
      index += i*n;
      n *= detail::axis_ref(a).nedges()+1;
    }, ii, _axes);
    return index;
  }

  template <typename... I>
  requires (sizeof...(I) != 1)
  index_type join_index(const I&... i) const {
    return join_index(std::array<index_type,sizeof...(I)>{index_type(i)...});
  }

  const bin_type& bin_at(index_type i) const { return detail::at(_bins,i); }
  bin_type& bin_at(index_type i) { return detail::at(_bins,i); }

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
  const bin_type& operator[](const T& ii) const {
    const index_type i = join_index(ii);
    if constexpr (map::Sizable<bins_container_type>)
      if (i >= _bins.size()) [[unlikely]]
        throw std::out_of_range("histogram bin index out of bound");
    return bin_at(i);
  }
  template <typename T = std::initializer_list<index_type>>
  bin_type& operator[](const T& ii) {
    const index_type i = join_index(ii);
    if constexpr (map::Sizable<bins_container_type>)
      if (i >= _bins.size()) [[unlikely]]
        throw std::out_of_range("histogram bin index out of bound");
    return bin_at(i);
  }

  // ----------------------------------------------------------------

  index_type find_bin_index(const map::Container auto& xs) const {
    index_type index = 0, n = 1;
    map::map([&](const auto& x, const auto& a) {
      index += detail::axis_ref(a).find_bin_index(x)*n;
      n *= detail::axis_ref(a).nedges()+1;
    }, xs, _axes);
    return index;
  }

  template <typename... T>
  requires (sizeof...(T) != 1) || (!map::Container<head_t<T...>>)
  index_type find_bin_index(const T&... xs) const {
    return find_bin_index(std::forward_as_tuple(xs...));
  }

  template <typename... T>
  const bin_type& find_bin(const T&... xs) const {
    return bin_at(find_bin_index(xs...));
  }
  template <typename... T>
  bin_type& find_bin(const T&... xs) {
    return bin_at(find_bin_index(xs...));
  }

  // ----------------------------------------------------------------

  template <typename I = std::initializer_list<index_type>, typename... Args>
  decltype(auto) fill_at(const I& ii, Args&&... args) {
    return filler_type::fill(bin_at(ii),std::forward<Args>(args)...);
  }

  decltype(auto) fill() { return filler_type::fill(find_bin()); }
  decltype(auto) operator()() { return filler_type::fill(find_bin()); }

  template <typename Coords, typename... Args>
  decltype(auto) fill(const Coords& xs, Args&&... args) {
    if constexpr (map::Container<Coords>)
      return filler_type::fill(find_bin(xs),std::forward<Args>(args)...);
    else
      return filler_type::fill(find_bin(xs,args...));
  }
  template <typename Coords, typename... Args>
  decltype(auto) operator()(const Coords& xs, Args&&... args) {
    return fill(xs,std::forward<Args>(args)...);
  }

  // Allow to brace-initialize coordinate arg
  // https://stackoverflow.com/q/60909588/2640636

  template <typename... Args,
    typename Coords = detail::coord_arg_t<head_t<Axes,Args...>> >
  decltype(auto) fill(const head_t<Coords>& xs, Args&&... args) {
    return filler_type::fill(find_bin(xs),std::forward<Args>(args)...);
  }
  template <typename... Args,
    typename Coords = detail::coord_arg_t<head_t<Axes,Args...>> >
  decltype(auto) operator()(const head_t<Coords>& xs, Args&&... args) {
    return fill(xs,std::forward<Args>(args)...);
  }

  template <typename Coord, typename... Args>
  requires (!detail::ValidCoordArg<Axes>)
  decltype(auto) fill(std::initializer_list<Coord> xs, Args&&... args) {
    return filler_type::fill(find_bin(xs),std::forward<Args>(args)...);
  }
  template <typename Coord, typename... Args>
  requires (!detail::ValidCoordArg<Axes>)
  decltype(auto) operator()(std::initializer_list<Coord> xs, Args&&... args) {
    return fill(xs,std::forward<Args>(args)...);
  }

};

} // end namespace ivanp::hist

#endif
