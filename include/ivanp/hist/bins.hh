#ifndef IVANP_HISTOGRAMS_BINS_HH
#define IVANP_HISTOGRAMS_BINS_HH

#include <ivanp/map/map.hh>

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
  ww2_bin& operator+=(const auto& weight) noexcept(noexcept(w += weight)) {
    w  += weight;
    w2 += weight*weight;
    return *this;
  }
  ww2_bin& operator+=(const ww2_bin& o) noexcept(noexcept(w += o.w)) {
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
  double moment() const noexcept { return std::get<I>(m); }
  template <>
  double moment<2>() const noexcept {
    return (n > 1) ? n*m[2]/((n-1)*m[0]) : 0.;
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
  counted_bin& operator+=(const counted_bin& o)
  noexcept(noexcept(bin += o.bin)) {
    bin += o.bin;
    n   += o.n;
    return *this;
  }
};

using mc_bin = counted_bin<ww2_bin<>>;

template <
  typename Bin,
  typename Weight = typename Bin::weight_type,
  typename Tag = void // in case multiple global weights are needed
>
struct static_weight_bin {
  using bin_type = Bin;
  using weight_type = Weight;

  bin_type bin;
  static weight_type weight;

  static_weight_bin operator++()
  noexcept(noexcept(bin += weight)) {
    bin += weight;
    return *this;
  }
  counted_bin& operator+=(const auto& weight)
  noexcept(noexcept(bin += weight)) {
    bin += weight;
    return *this;
  }
  static_weight_bin& operator+=(const static_weight_bin& o)
  noexcept(noexcept(bin += o.bin)) {
    return *this += o.bin;
  }
};

template <typename Weights>
struct multiweight_bin {
  using weights_container_type = Weights;

  weights_container_type weights;
  // TODO: how to call a non-default constructor?
  // maybe specialize static_weight_bin<multiweight_bin<std::vector<...>>,...>

  multiweight_bin& operator++() {
    ivanp::map::map([](auto& w){ ++w; }, weights);
    return *this;
  }
  multiweight_bin& operator+=(const auto& arg) {
    ivanp::map::map([](auto& w, auto& a){ w += a; }, weights, arg);
    return *this;
  }
  template <typename T>
  multiweight_bin& operator+=(const multiweight_bin<T>& o)
  noexcept(noexcept(bin += o.bin)) {
    return *this += o.weights;
  }
};

template <typename Bin, typename Weight = typename Bin::weight_type>
struct nlo_bin {
  using bin_type = Bin;
  using weight_type = Weight;

  bin_type bin;
  weight_type wsum { };
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
  nlo_bin& operator+=(const nlo_bin& o) noexcept {
    if (wsum!=0) finalize();
    if (o.wsum!=0) bin += o.wsum;
    bin += o.bin;
    nent += o.nent;
    return *this;
  }
};

using nlo_mc_bin = nlo_bin<mc_bin>;
using nlo_mc_multibin =
  nlo_bin<
    counted_bin< static_weight_bin<
      multiweight_bin< std::vector<ww2_bin<double>> >,
      multiweight_bin< std::vector<double> >
    > >,
    multiweight_bin< std::vector<double> >
  >;

} // end namespace ivanp::hist

#endif
