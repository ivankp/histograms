#ifndef IVANP_HISTOGRAMS_HH
#define IVANP_HISTOGRAMS_HH

#include <type_traits>
#include <tuple>
#include <array>
#include <vector>
#include <string>

#include <ivanp/cont/general.hh>
#include <ivanp/cont/map.hh>
#include <ivanp/enum_class_bitmask.hh>
#include <ivanp/hist/axes.hh>
#include <ivanp/hist/bin_filler.hh>

namespace ivanp::hist {
enum class hist_flags {
  none = 0,
  perbin_axes = 1 << 0,
};
}

namespace ivanp {
template <>
constexpr bool enable_bitmask_operators<hist::hist_flags> = true;
}

namespace ivanp::hist {

namespace detail {

template <typename Axes, bool perbin_axes>
struct coord_arg { };

template <typename Axes>
requires requires { typename Axes::value_type; }
struct coord_arg<Axes,false> {
  using type = std::initializer_list<
    axis_edge_type<typename Axes::value_type> >;
};
template <typename Axes>
requires requires { typename Axes::value_type; }
struct coord_arg<Axes,true> {
  using type = std::initializer_list<
    axis_edge_type<cont::first_type<typename Axes::value_type>> >;
};

template <typename... Axes>
struct coord_arg<std::tuple<Axes...>,false> {
  using type = std::tuple<axis_edge_type<Axes>...>;
};
template <typename... Axes>
struct coord_arg<std::tuple<Axes...>,true> {
  using type = std::tuple<axis_edge_type<cont::first_type<Axes>>...>;
};

template <typename Axes, size_t N>
struct coord_arg<std::array<Axes,N>,false> {
  using type = std::array<axis_edge_type<Axes>,N>;
};
template <typename Axes, size_t N>
struct coord_arg<std::array<Axes,N>,true> {
  using type = std::array<axis_edge_type<cont::first_type<Axes>>,N>;
};

template <typename Axes, bool perbin_axes>
using coord_arg_t = typename coord_arg<Axes,perbin_axes>::type;

template <typename Axes, bool perbin_axes>
concept ValidCoordArg = requires {
  typename coord_arg<Axes,perbin_axes>::type;
};

} // end namespace detail

namespace impl {

template <
  typename Bin,
  cont::Container Axes,
  cont::Container Bins,
  typename Filler,
  hist_flags flags
>
class histogram {
public:
  using index_type = hist::index_type;
  using bin_type = Bin;
  using axes_type = Axes;
  using bins_type = Bins;
  using filler_type = Filler;
  static constexpr bool perbin_axes = !!(flags & hist_flags::perbin_axes);

private:
  axes_type _axes;
  bins_type _bins;

  template <typename... T>
  void resize_bins(T&&... bin_args) {
    static constexpr bool can_resize =
      requires (index_type n) { _bins.resize(n); };
    static constexpr bool can_emplace_back = requires {
      _bins.emplace_back(*this,index_type{},std::forward<T>(bin_args)...); };
    static constexpr bool can_emplace = requires {
      _bins.emplace(*this,index_type{},std::forward<T>(bin_args)...); };

    if constexpr (
      can_resize || can_emplace_back || can_emplace
    ) {
      index_type n = 1;
      if constexpr (!perbin_axes) {
        cont::map([&n](const auto& a) {
          n *= get_axis_ref(a).nbins();
        }, _axes);
      } else {
        cont::map([&]<typename D>(const D& dim) {
          static_assert(cont::List<D>,
            "cannot use perbin_axes with non-iterable axis containers");
          const index_type
            na = std::size(dim),
            j  = n < na ? n : na-1,
            dj = n - j;
          n = 0;
          auto it = std::begin(dim);
          for (index_type k = 0; k<j; ++k, ++it)
            n += get_axis_ref(*it).nbins();
          n += dj * (get_axis_ref(*it).nbins());
        }, _axes);
      }
      if constexpr (sizeof...(bin_args)==0) {
        if constexpr (can_resize) _bins.resize(n);
      } else {
        if constexpr (can_emplace_back || can_emplace) {
          if constexpr (requires { _bins.reserve(n); })
            _bins.reserve(n);
          for (index_type i=0; i<n; ++i) {
            if constexpr (can_emplace_back)
              _bins.emplace_back(*this,i,std::forward<T>(bin_args)...);
            else
              _bins.emplace(*this,i,std::forward<T>(bin_args)...);
          }
        } else {
          if constexpr (can_resize) _bins.resize(n);
          for (index_type i=0; i<n; ++i)
            _bins[i] = bin_type(*this,i,std::forward<T>(bin_args)...);
        }
      }
    }
  }

public:
  histogram() = default;
  histogram(const histogram&) = default;
  histogram(histogram&&) = default;
  histogram& operator=(const histogram&) = default;
  histogram& operator=(histogram&&) = default;
  ~histogram() = default;

  template <typename... T>
  explicit histogram(const axes_type& axes, T&&... bin_args)
  noexcept(std::is_nothrow_copy_constructible_v<axes_type>)
  : _axes(axes)
  {
    resize_bins(std::forward<T>(bin_args)...);
  }
  template <typename... T>
  explicit histogram(axes_type&& axes, T&&... bin_args)
  noexcept(std::is_nothrow_move_constructible_v<axes_type>)
  : _axes(std::move(axes))
  {
    resize_bins(std::forward<T>(bin_args)...);
  }

  template <cont::Container C, typename... T>
  requires(requires { (axes_type)std::declval<C&&>(); })
  explicit histogram(C&& axes, T&&... bin_args)
  : histogram((axes_type)std::forward<C>(axes), std::forward<T>(bin_args)...)
  { }

  template <cont::Container C, typename... T>
  explicit histogram(C&& axes, T&&... bin_args) {
    if constexpr (cont::Tuple<C>) {
      cont::map<cont::map_flags::forward>(
        []<typename A, typename B>(A& a, B&& b){
          a = (A)std::forward<B>(b);
        }, _axes, axes);
    } else {
      // TODO: non-stl case
      if constexpr (requires { _axes.reserve(size_t{}); })
        _axes.reserve(cont::size(axes));
      cont::map<cont::map_flags::forward>([&]<typename A>(A&& a){
        if constexpr (requires { _axes.emplace_back(std::forward<A>(a)); }) {
          _axes.emplace_back(std::forward<A>(a));
        } else {
          _axes.emplace(std::forward<A>(a));
        }
      }, axes);
    }
    resize_bins(std::forward<T>(bin_args)...);
  }

  const axes_type& axes() const noexcept { return _axes; }
  template <size_t I>
  const auto& axis() const noexcept { return std::get<I>(_axes); }
  const auto& axis(index_type i) const noexcept { return cont::at(_axes,i); }
  auto ndim() const noexcept { return cont::size(_axes); }

  const bins_type& bins() const noexcept { return _bins; }
  bins_type& bins() noexcept { return _bins; }
  auto nbins() const noexcept { return cont::size(_bins); }
  auto size() const noexcept { return cont::size(_bins); }

  auto begin() const noexcept { return _bins.begin(); }
  auto begin() noexcept { return _bins.begin(); }
  auto   end() const noexcept { return _bins.end(); }
  auto   end() noexcept { return _bins.end(); }

  // ----------------------------------------------------------------

  index_type join_index(index_type i) const { return i; }

  template <typename T = std::initializer_list<index_type>>
  requires cont::Container<T>
  index_type join_index(const T& ii) const {
    // N = (a*nb + b)*nc + c
    index_type index = 0;
    if constexpr (!perbin_axes) {
      cont::map([&](index_type i, const auto& a) {
        (index *= get_axis_ref(a).nbins()) += i;
      }, ii, _axes);
    } else {
      cont::map([&]<typename D>(index_type i, const D& dim) {
        static_assert(cont::List<D>,
          "cannot use perbin_axes with non-iterable axis containers");
        const index_type
          nd = std::size(dim),
          j  = index < nd ? index : nd-1,
          dj = index - j;
        index = 0;
        auto it = std::begin(dim);
        for (index_type k = 0; k<j; ++k, ++it)
          index += get_axis_ref(*it).nbins();
        const auto& a = get_axis_ref(*it);
        index += (dj * (a.nbins())) + i;
      }, ii, _axes);
    }
    return index;
  }

  template <typename... I>
  requires (sizeof...(I) != 1)
  index_type join_index(const I&... i) const {
    return join_index(std::array<index_type,sizeof...(I)>{index_type(i)...});
  }

  const bin_type& bin_at(index_type i) const { return cont::at(_bins,i); }
  bin_type& bin_at(index_type i) { return cont::at(_bins,i); }

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
    if constexpr (cont::Sizable<bins_type>)
      if (i >= _bins.size()) [[unlikely]]
        throw std::out_of_range("histogram bin index out of bound");
    return bin_at(i);
  }
  template <typename T = std::initializer_list<index_type>>
  bin_type& operator[](const T& ii) {
    const index_type i = join_index(ii);
    if constexpr (cont::Sizable<bins_type>)
      if (i >= _bins.size()) [[unlikely]]
        throw std::out_of_range("histogram bin index out of bound");
    return bin_at(i);
  }

  // ----------------------------------------------------------------

  index_type find_bin_index(const cont::Container auto& xs) const {
    // N = (a*nb + b)*nc + c
    index_type index = 0;
    if constexpr (!perbin_axes) {
      cont::map([&](const auto& x, const auto& _a) {
        const auto& a = get_axis_ref(_a);
        (index *= a.nbins()) += a.find_bin_index(x);
      }, xs, _axes);
    } else {
      cont::map([&]<typename D>(const auto& x, const D& dim) {
        static_assert(cont::List<D>,
          "cannot use perbin_axes with non-iterable axis containers");
        const index_type
          nd = std::size(dim),
          j  = index < nd ? index : nd-1,
          dj = index - j;
        index = 0;
        auto it = std::begin(dim);
        for (index_type k = 0; k<j; ++k, ++it)
          index += get_axis_ref(*it).nbins();
        const auto& a = get_axis_ref(*it);
        index += (dj * (a.nbins())) + a.find_bin_index(x);
      }, xs, _axes);
    }
    return index;
  }

  template <typename... T>
  requires (sizeof...(T) != 1) || (!cont::Container<head_t<T...>>)
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
    if constexpr (cont::Container<Coords>)
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
    typename Coords = detail::coord_arg_t<head_t<Axes,Args...>,perbin_axes> >
  decltype(auto) fill(const head_t<Coords>& xs, Args&&... args) {
    return filler_type::fill(find_bin(xs),std::forward<Args>(args)...);
  }
  template <typename... Args,
    typename Coords = detail::coord_arg_t<head_t<Axes,Args...>,perbin_axes> >
  decltype(auto) operator()(const head_t<Coords>& xs, Args&&... args) {
    return fill(xs,std::forward<Args>(args)...);
  }

  template <typename Coord, typename... Args>
  requires (!detail::ValidCoordArg<Axes,perbin_axes>)
  decltype(auto) fill(std::initializer_list<Coord> xs, Args&&... args) {
    return filler_type::fill(find_bin(xs),std::forward<Args>(args)...);
  }
  template <typename Coord, typename... Args>
  requires (!detail::ValidCoordArg<Axes,perbin_axes>)
  decltype(auto) operator()(std::initializer_list<Coord> xs, Args&&... args) {
    return fill(xs,std::forward<Args>(args)...);
  }

};

} // end namespace impl

template <typename> struct bins_spec { };
template <typename> struct is_bins_spec {
  static constexpr bool value = false;
};
template <typename T>
struct is_bins_spec<bins_spec<T>> {
  static constexpr bool value = true;
  using type = T;
};

template <typename> struct axes_spec { };
template <typename> struct is_axes_spec {
  static constexpr bool value = false;
};
template <typename T>
struct is_axes_spec<axes_spec<T>> {
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

template <hist_flags... flags> struct flags_spec {
  static constexpr hist_flags value = (hist_flags::none | ... | flags);
};
template <typename> struct is_flags_spec {
  static constexpr bool value = false;
  using type = flags_spec<>;
};
template <hist_flags... flags>
struct is_flags_spec<flags_spec<flags...>> {
  static constexpr bool value = true;
  using type = flags_spec<flags...>;
};

template <typename Bin = double, typename... Specs>
using histogram = impl::histogram<
  Bin,
  typename std::disjunction<
    is_axes_spec<Specs>...,
    is_axes_spec<axes_spec< std::vector<cont_axis<std::vector<double>>> >>
  >::type,
  typename std::disjunction<
    is_bins_spec<Specs>...,
    is_bins_spec<bins_spec< std::vector<Bin> >>
  >::type,
  typename std::disjunction<
    is_filler_spec<Specs>...,
    is_filler_spec<filler_spec< bin_filler >>
  >::type,
  ( hist_flags::none | ... | is_flags_spec<Specs>::type::value )
>;

template <typename>
struct is_histogram: std::false_type { };
template <
  typename Bin, typename Axes, typename Bins, typename Filler,
  hist_flags flags >
struct is_histogram<impl::histogram<Bin,Axes,Bins,Filler,flags>>
: std::true_type { };

template <typename T>
concept Histogram = is_histogram<std::decay_t<T>>::value;

} // end namespace ivanp::hist

#endif
