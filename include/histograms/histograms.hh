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
    using namespace containers;
    if constexpr (is_resizable<bins_container_type,size_t>::value) {
      index_type n = 1;
      for_each([&](const auto& a){ n *= a.nedges()+1; },_axes);
      _bins.resize(n);
    }
  }

public:
  histogram() { }
  histogram(const histogram&) = default;
  histogram(histogram&&) = default;
  histogram& operator=(const histogram&) = default;
  histogram& operator=(histogram&&) = default;

  histogram(const axes_type& axes): _axes(axes) { resize_bins(); }
  histogram(axes_type&& axes): _axes(std::move(axes)) { resize_bins(); }

  const axes_type& axes() const noexcept { return _axes; }
  const bins_container_type& bins() const noexcept { return _bins; }
  bins_container_type& bins() noexcept { return _bins; }

  auto begin() const noexcept { return _bins.begin(); }
  auto begin() noexcept { return _bins.begin(); }
  auto   end() const noexcept { return _bins.end(); }
  auto   end() noexcept { return _bins.end(); }

  const bin_type& bin_at(index_type i) const {
    return containers::at(_bins,i);
  }
  bin_type& bin_at(index_type i) {
    return containers::at(_bins,i);
  }

  template <typename T = std::initializer_list<index_type>>
  auto join_index(const T& ii) const
  -> std::enable_if_t< containers::has_either_size<T>::value, index_type >
  {
    index_type index = 0, n = 1;
    containers::for_each([&](index_type i, const auto& a){
      index += i*n;
      n *= a.nedges()+1;
    },ii,_axes);
    return index;
  }

  index_type join_index(index_type i) const { return i; }
  template <typename... T>
  auto join_index(const T&... ii) const
  -> std::enable_if_t< (sizeof...(ii) > 1), index_type > {
    return join_index(std::array<index_type,sizeof...(ii)>{index_type(ii)...});
  }

  template <typename T = std::initializer_list<index_type>, typename... T2>
  const bin_type& bin_at(const T& ii, const T2&... ii2) const {
    return bin_at(join_index(ii,ii2...));
  }
  template <typename T = std::initializer_list<index_type>, typename... T2>
  bin_type& bin_at(const T& ii, const T2&... ii2) {
    return bin_at(join_index(ii,ii2...));
  }

  template <typename T = std::initializer_list<index_type>>
  const bin_type& operator[](const T& ii) const { return bin_at(ii); }
  template <typename T = std::initializer_list<index_type>>
  bin_type& operator[](const T& ii) { return bin_at(ii); }

  template <typename T>
  auto find_bin_index(const T& xs) const -> std::enable_if_t<
    containers::has_either_size<const T>::value, index_type
  > {
    index_type index = 0, n = 1;
    containers::for_each([&](const auto& x, const auto& a){
      index += a.find_bin_index(x)*n;
      n *= a.nedges()+1;
    },xs,_axes);
    return index;
  }

  template <typename... T>
  auto find_bin_index(const T&... xs) const -> std::enable_if_t<
    (sizeof...(xs) > 1)
    || !containers::has_either_size<const head_t<T...>>::value,
    index_type
  > {
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

  template <typename T = std::initializer_list<index_type>, typename... Args>
  decltype(auto) fill_at(const T& i, Args&&... args) {
    return filler_type::fill(bin_at(i),std::forward<Args>(args)...);
  }

  template <typename T, typename... Args>
  decltype(auto) fill(const T& xs, Args&&... args) {
    return filler_type::fill(find_bin(xs),std::forward<Args>(args)...);
  }

  template <typename... T>
  decltype(auto) operator()(T&&... xs) { return fill(std::forward<T>(xs)...); }
};

} // end namespace histograms

#endif
