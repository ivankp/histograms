#ifndef IVANP_HISTOGRAMS_BINS_HH
#define IVANP_HISTOGRAMS_BINS_HH

#include <ivanp/map/map.hh>
#include <cmath>

namespace ivanp::hist {

template <typename Weight = double>
struct ww2_bin {
  using weight_type = Weight;

  weight_type w = 0, w2 = 0;
  ww2_bin& operator++() noexcept(noexcept(++w)) {
    ++w;
    ++w2;
    return *this;
  }
  ww2_bin& operator+=(const auto& weight)
  noexcept(noexcept(w += weight)) {
    w  += weight;
    w2 += weight*weight;
    return *this;
  }
  ww2_bin& operator+=(const ww2_bin<auto>& o)
  noexcept(noexcept(w += o.w)) {
    w  += o.w;
    w2 += o.w2;
    return *this;
  }
};

template <unsigned MaxMoment=2>
struct stat_bin {
  long unsigned n = 0;
  std::array<double,MaxMoment+1> m { }; // moments' accumulators
  // 0: total
  // 1: mean
  // 2: variance
  // 3: skewness
  // 4: kurtosis

  // https://www.johndcook.com/blog/skewness_kurtosis/

  void operator()(double x) noexcept {
    ++m[0];
    if constexpr (MaxMoment > 0) {
      if (n == 0) {
        m[1] = x;
      } else {
        const double d = x-m[1];
        m[1] += d/m[0];
        if constexpr(MaxMoment > 1) {
          m[2] += d*(x-m[1]);
        }
      }
    }
    ++n;
  }
  void operator()(double x, double weight) noexcept {
    m[0] += weight;
    if constexpr (MaxMoment > 0) {
      if (n == 0) {
        m[1] = x;
      } else {
        const double wd = weight*(x-m[1]);
        m[1] += wd/m[0];
        if constexpr(MaxMoment > 1) {
          m[2] += wd*(x-m[1]);
        }
      }
    }
    ++n;
  }

  template <unsigned I>
  double moment() const noexcept {
    if constexpr (I==2)
      return (n > 1) ? n*m[2]/((n-1)*m[0]) : 0.;
    else
      return std::get<I>(m);
  }

  double total    () const noexcept { return moment<0>(); }
  double mean     () const noexcept { return moment<1>(); }
  double variance () const noexcept { return moment<2>(); }
  double skewness () const noexcept { return moment<3>(); }
  double kurtosis () const noexcept { return moment<4>(); }

  double stdev    () const noexcept { return std::sqrt(variance()); }
};

template <typename Bin, typename Count = long unsigned>
struct counted_bin {
  using bin_type = Bin;
  using count_type = Count;
  using weight_type = typename bin_type::weight_type; // might need to default

  bin_type bin;
  count_type n;

  counted_bin& operator++()
  noexcept(noexcept(++bin)) {
    ++bin;
    ++n;
    return *this;
  }
  counted_bin& operator+=(const auto& weight)
  noexcept(noexcept(bin += weight)) {
    bin += weight;
    ++n;
    return *this;
  }
  counted_bin& operator+=(const counted_bin<auto,auto>& o)
  noexcept(noexcept(bin += o.bin)) {
    bin += o.bin;
    n   += o.n;
    return *this;
  }
};

template <
  typename Bin,
  typename Weight = typename Bin::weight_type,
  typename Tag = void // in case multiple global weights are needed
>
struct static_weight_bin {
  using bin_type = Bin;
  using weight_type = Weight;

  bin_type bin;
  inline static weight_type weight = []() -> weight_type {
    if constexpr (std::is_arithmetic_v<weight_type>) return 1;
    else return { };
  }();

  static_weight_bin() = default;
  static_weight_bin() requires requires {
    bin->resize(weight->size());
  } {
    bin->resize(weight->size());
  }

  static_weight_bin& operator+=(const auto& weight)
  noexcept(noexcept(bin += weight)) {
    bin += weight;
    return *this;
  }
  static_weight_bin operator++()
  noexcept(noexcept(bin += weight)) {
    return *this += weight;
  }
  static_weight_bin& operator+=(const static_weight_bin<auto,auto,auto>& o)
  noexcept(noexcept(bin += o.bin)) {
    return *this += o.bin;
  }
};

template <typename T>
struct multi_bin {
  T xs;

  multi_bin& operator++() noexcept {
    ivanp::map::map([](auto& x){ ++x; }, xs);
    return *this;
  }
  multi_bin& operator+=(const auto& arg) noexcept {
    ivanp::map::map([](auto& x, auto& a){ x += a; }, xs, arg);
    return *this;
  }
  multi_bin& operator+=(const multi_bin<auto>& o) noexcept {
    return *this += o.xs;
  }

  T operator->() noexcept { return &xs; }
  const T operator->() const noexcept { return &xs; }
  T& operator*() noexcept { return xs; }
  const T& operator*() const noexcept { return xs; }
};

template <typename Bin, typename Weight = typename Bin::weight_type>
struct nlo_bin {
  using bin_type = Bin;
  using weight_type = Weight;

  bin_type bin;
  weight_type wsum;
  long unsigned nent = 0;
  static long unsigned id, prev_id;

  nlo_bin& operator++() noexcept {
    if (nent==0) [[unlikely]] { prev_id = id; }
    if (id == prev_id) ++wsum;
    else {
      prev_id = id;
      bin += wsum;
      wsum = 1;
    }
    ++nent;
    return *this;
  }
  nlo_bin& operator+=(const auto& weight) noexcept {
    if (nent==0) [[unlikely]] { prev_id = id; }
    if (id == prev_id) wsum += weight;
    else {
      prev_id = id;
      bin += wsum;
      wsum = weight;
    }
    ++nent;
    return *this;
  }
  nlo_bin& finalize() noexcept {
    bin += wsum;
    wsum = 0;
    return *this;
  }
  nlo_bin& operator+=(const nlo_bin<auto,auto>& o) noexcept {
    if (wsum!=0) finalize();
    if (o.wsum!=0) bin += o.wsum;
    bin += o.bin;
    nent += o.nent;
    return *this;
  }
};

// ==================================================================

using mc_bin = counted_bin< static_weight_bin< ww2_bin<double> > >;

using nlo_mc_bin = nlo_bin<mc_bin>;

using nlo_mc_multibin =
  nlo_bin<
    counted_bin< static_weight_bin<
      multi_bin< std::vector<ww2_bin<double>> >,
      multi_bin< std::vector<double> >
    > >,
    multi_bin< std::vector<double> >
  >;

} // end namespace ivanp::hist

#endif
