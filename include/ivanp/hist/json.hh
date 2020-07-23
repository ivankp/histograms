#ifndef IVANP_HISTOGRAMS_JSON_HH
#define IVANP_HISTOGRAMS_JSON_HH

#ifndef IVANP_HISTOGRAMS_HH
#error "must include histograms.hh first"
#else

#include <nlohmann/json.hpp>

// https://github.com/ivankp/histograms#histograms
// https://github.com/nlohmann/json#how-do-i-convert-third-party-types

namespace ivanp::hist {

template <typename> const char* bin_def();

template <typename List, bool Poly, typename Edge>
void to_json(nlohmann::json& j, const list_axis<List,Poly,Edge>& axis) {
  j = axis.edges();
}

template <Histogram H>
void to_json(nlohmann::json& j, const H& h) {
  j = { {"axes", h.axes()} };
  using bin_type = typename H::bin_type;
  auto& bins = j["bins"];
  if constexpr (std::is_arithmetic_v<bin_type>)
    bins = h.bins();
  else
    bins = { nlohmann::json::parse(bin_def<bin_type>()), h.bins() };
}

// void from_json(const json& j, Histogram auto& h) {
//   j.at("name").get_to(p.name);
//   j.at("address").get_to(p.address);
//   j.at("age").get_to(p.age);
// }

template <typename T>
concept HistogramDict = requires (T& hs) {
  {  std::get<0>(*hs.begin()) } -> ivanp::stringlike;
  { *std::get<1>(*hs.begin()) } -> ivanp::hist::Histogram;
};

nlohmann::json to_json(const HistogramDict auto& hs) {
  using nlohmann::json;
  json axes  = json::array(),
       bins  = json::array(),
       hists = json::object();
  for (const auto& [name, h_ptr] : hs) {
    auto& h = hists[name] = *h_ptr;
    for (auto& ha : h["axes"]) {
      unsigned i = 0, n = axes.size();
      for (; i<n; ++i)
        if (ha == axes[i]) break;
      if (i==n) axes.push_back(std::move(ha));
      ha = i;
    }
    auto& hbins = h["bins"];
    if (hbins.size()==2 && hbins[0].is_array()) {
      auto& hb = hbins[0];
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
void to_json(nlohmann::json& j, const mc_bin& b) {
  j = { b.w, b.w2, b.n };
}
template <>
constexpr const char* bin_def<mc_bin>() {
  return R"(["w","w2","n"])";
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
