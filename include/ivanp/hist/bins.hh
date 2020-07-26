#ifndef IVANP_HISTOGRAMS_BINS_HH
#define IVANP_HISTOGRAMS_BINS_HH

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

template <typename Weight = double, typename Count = long unsigned>
struct mc_bin {
  using weight_type = Weight;
  using count_type = Count;

  weight_type w = 0, w2 = 0;
  count_type n = 0;
  mc_bin& operator++() noexcept(noexcept(++w)) {
    ++w;
    ++w2;
    ++n;
    return *this;
  }
  mc_bin& operator+=(const auto& weight)
  noexcept(noexcept(w += weight)) {
    w  += weight;
    w2 += weight*weight;
    ++n;
    return *this;
  }
  mc_bin& operator+=(const mc_bin<auto,auto>& o)
  noexcept(noexcept(w += o.w)) {
    w  += o.w;
    w2 += o.w2;
    n += o.n;
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

struct nlo_mc_multibin {
  std::vector<ww2_bin<double>> ww2;
  std::vector<double> wsum;
  inline static std::vector<double> weight;
  int prev_id = -1;
  inline static int id;
  long unsigned n=0, nent=0;

  nlo_mc_multibin(): ww2(weight.size()), wsum(weight.size()) { }
  nlo_mc_multibin& operator++() noexcept {
    if (nent==0) [[unlikely]] {
      prev_id = id;
      ++n;
    }
    if (prev_id != id) [[likely]] {
      prev_id = id;
      for (unsigned i=0, n=weight.size(); i<n; ++i) {
        auto& w = wsum[i];
        ww2[i] += w;
        w = weight[i];
      }
      ++n;
    } else {
      for (unsigned i=0, n=weight.size(); i<n; ++i)
        wsum[i] += weight[i];
    }
    ++nent;
    return *this;
  }
  nlo_mc_multibin& finalize() noexcept {
    for (unsigned i=0, n=weight.size(); i<n; ++i) {
      auto& w = wsum[i];
      ww2[i] += w;
      w = 0;
    }
    return *this;
  }
};

} // end namespace ivanp::hist

#endif
