#ifndef IVANP_HISTOGRAMS_JSON_HH
#define IVANP_HISTOGRAMS_JSON_HH

#ifndef IVANP_HISTOGRAMS_HH
#error "must include histograms.hh first"
#else

#include <nlohmann/json.hpp>

// https://github.com/ivankp/histograms#histograms
// https://github.com/nlohmann/json#how-do-i-convert-third-party-types

namespace ivanp::hist {

template <typename>
struct bin_def {
  static nlohmann::json def() noexcept { return nullptr; }
};

template <typename List, bool Poly, typename Edge>
void to_json(nlohmann::json& j, const list_axis<List,Poly,Edge>& axis) {
  j = axis.edges();
}

template <Histogram H>
void to_json(nlohmann::json& j, const H& h) {
  j = { {"axes", h.axes()} };
  using bin_type = typename H::bin_type;
  j["bins"] = { bin_def<bin_type>::def(), h.bins() };
}

template <typename T>
concept HistogramDict = requires (T& hs) {
  {  std::get<0>(*hs.begin()) } -> convertible_to<std::string>;
  { *std::get<1>(*hs.begin()) } -> Histogram;
};

nlohmann::json to_json(const HistogramDict auto& hs) {
  using hist_t = std::decay_t<decltype(*std::get<1>(*hs.begin()))>;
  using nlohmann::json;
  json axes  = json::array(),
       bins  = json::array(),
       hists = json::object();
  for (const auto& [name, h_ptr] : hs) {
    auto& h = hists[name] = *h_ptr;
    for (auto& a : h["axes"]) {
      if constexpr (!hist_t::perbin_axes) {
        unsigned i = 0, n = axes.size();
        for (; i<n; ++i)
          if (a == axes[i]) break;
        if (i==n) axes.push_back(std::move(a));
        a = i;
      } else {
        for (auto& a : a) {
          unsigned i = 0, n = axes.size();
          for (; i<n; ++i)
            if (a == axes[i]) break;
          if (i==n) axes.push_back(std::move(a));
          a = i;
        }
      }
    }
    auto& hb = h["bins"][0];
    if (!hb.is_null()) {
      unsigned i = 0, n = bins.size();
      for (; i<n; ++i)
        if (hb == bins[i]) break;
      if (i==n) bins.push_back(std::move(hb));
      hb = i;
    }
  }
  return {
    { "axes" , std::move(axes ) },
    { "bins" , std::move(bins ) },
    { "hists", std::move(hists) }
  };
}

#ifdef IVANP_HISTOGRAMS_BINS_HH

void to_json(nlohmann::json& j, const ww2_bin<auto>& b) {
  j = { b.w, b.w2 };
}
template <typename T>
struct bin_def<ww2_bin<T>> {
  static nlohmann::json def() noexcept {
    return R"(["w","w2"])"_json;
  }
};

void to_json(nlohmann::json& j, const mc_bin<auto,auto>& b) {
  j = { b.w, b.w2, b.n };
}
template <typename T, typename C>
struct bin_def<mc_bin<T,C>> {
  static nlohmann::json def() noexcept {
    return R"(["w","w2","n"])"_json;
  }
};

void to_json(nlohmann::json& j, const nlo_mc_multibin& b) {
  j = { b.ww2, b.n, b.nent };
}

#endif

} // end namespace ivanp::hist

namespace nlohmann {

template <ivanp::hist::HistogramDict T>
struct adl_serializer<T> {
  static void to_json(json& j, const T& hs) {
    j = ivanp::hist::to_json(hs);
  }
};

} // end namespace nlohmann

#endif
#endif
