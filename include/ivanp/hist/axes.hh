#ifndef IVANP_HISTOGRAMS_AXES_HH
#define IVANP_HISTOGRAMS_AXES_HH

#include <type_traits>
#include <limits>
#include <algorithm>
#include <iterator>
#include <variant>

namespace ivanp::hist {

#ifndef IVANP_HIST_INDEX_TYPE
#define IVANP_HIST_INDEX_TYPE unsigned
#endif
using index_type = IVANP_HIST_INDEX_TYPE;

// Container axis ===================================================
template <
  typename Cont = std::vector<double>,
  typename Edge = typename std::remove_reference_t<Cont>::value_type
>
class cont_axis {
public:
  using edge_type = Edge;
  using cont_type = Cont;

  static constexpr edge_type lowest =
    std::numeric_limits<edge_type>::has_infinity
    ? -std::numeric_limits<edge_type>::infinity()
    : std::numeric_limits<edge_type>::lowest();

  static constexpr edge_type highest =
    std::numeric_limits<edge_type>::has_infinity
    ? std::numeric_limits<edge_type>::infinity()
    : std::numeric_limits<edge_type>::max();

private:
  cont_type _edges;

public:
  cont_axis() noexcept = default;
  cont_axis(const cont_axis&) noexcept = default;
  cont_axis(cont_axis&&) noexcept = default;
  cont_axis& operator=(const cont_axis&) noexcept = default;
  cont_axis& operator=(cont_axis&&) noexcept = default;
  ~cont_axis() = default;

  cont_axis(const cont_type& edges): _edges(edges) { }
  cont_axis(cont_type&& edges)
  noexcept(std::is_nothrow_move_constructible_v<cont_type>)
  : _edges(std::move(edges)) { }
  cont_axis& operator=(const cont_type& edges) {
    _edges = edges;
    return *this;
  }
  cont_axis& operator=(cont_type&& edges)
  noexcept(std::is_nothrow_move_assignable_v<cont_type>) {
    _edges = std::move(edges);
    return *this;
  }

  cont_axis(std::initializer_list<edge_type> edges): _edges(edges) { }
  cont_axis& operator=(std::initializer_list<edge_type> edges) {
    _edges = edges;
    return *this;
  }

  index_type nbins () const noexcept { return _edges.size()+1; }
  index_type ndiv  () const noexcept { return _edges.size()-1; }
  index_type nedges() const noexcept { return _edges.size()  ; }

  edge_type edge(index_type i) const noexcept { return _edges[i]; }
  edge_type operator[](index_type i) const noexcept { return edge(i); }

  edge_type min() const noexcept(noexcept(_edges.front()))
  { return _edges.front(); }
  edge_type max() const noexcept(noexcept(_edges.back()))
  { return _edges.back(); }

  edge_type lower(index_type i) const noexcept {
    return i==0 ? lowest : i>nedges() ? highest : edge(i-1);
  }
  edge_type upper(index_type i) const noexcept {
    return i>=nedges() ? highest : edge(i);
  }

  index_type find_bin_index(edge_type x) const noexcept {
    using namespace std;
    return distance(
      begin(_edges), upper_bound(begin(_edges), end(_edges), x)
    );
  }
  index_type operator()(edge_type x) const noexcept {
    return find_bin_index(x);
  }

  cont_type& edges() noexcept { return _edges; }
  const cont_type& edges() const noexcept { return _edges; }
};

// Uniform axis =====================================================
template <typename Edge = double>
class uniform_axis {
public:
  using edge_type = Edge;

  static constexpr edge_type lowest =
    std::numeric_limits<edge_type>::has_infinity
    ? -std::numeric_limits<edge_type>::infinity()
    : std::numeric_limits<edge_type>::lowest();

  static constexpr edge_type highest =
    std::numeric_limits<edge_type>::has_infinity
    ? std::numeric_limits<edge_type>::infinity()
    : std::numeric_limits<edge_type>::max();

private:
  index_type _ndiv;
  edge_type _min, _max;

public:
  uniform_axis() noexcept = default;
  uniform_axis(const uniform_axis&) noexcept = default;
  uniform_axis(uniform_axis&&) noexcept = default;
  uniform_axis& operator=(const uniform_axis&) noexcept = default;
  uniform_axis& operator=(uniform_axis&&) noexcept = default;
  ~uniform_axis() = default;

  uniform_axis(index_type ndiv, edge_type min, edge_type max) noexcept
  : _ndiv(ndiv), _min(min), _max(max)
  {
    if (_max < _min) std::swap(_min,_max);
  }

  index_type nbins () const noexcept { return _ndiv+2; }
  index_type ndiv  () const noexcept { return _ndiv  ; }
  index_type nedges() const noexcept { return _ndiv+1; }

  edge_type edge(index_type i) const noexcept {
    return _min + i*((_max - _min)/_ndiv);
  }
  edge_type operator[](index_type i) const noexcept { return edge(i); }

  edge_type min() const noexcept { return _min; }
  edge_type max() const noexcept { return _max; }

  edge_type lower(index_type i) const noexcept {
    if (i==0) return lowest;
    if (i >= _ndiv+1) return highest;
    return edge(i-1);
  }
  edge_type upper(index_type i) const noexcept {
    if (i > _ndiv) return highest;
    return edge(i);
  }

  index_type find_bin_index(edge_type x) const noexcept {
    if (x < _min) return 0;
    if (!(x < _max)) return _ndiv+1;
    return index_type(_ndiv*(x-_min)/(_max-_min)) + 1;
  }
  index_type operator()(edge_type x) const noexcept {
    return find_bin_index(x);
  }
};

// Variant axis =====================================================
template <typename... Axes>
class variant_axis {
public:
  using edge_type = std::common_type_t<typename Axes::edge_type...>;

private:
  std::variant<Axes...> ax;

public:
  variant_axis() noexcept = default;
  variant_axis(const variant_axis&) noexcept = default;
  variant_axis(variant_axis&&) noexcept = default;
  variant_axis& operator=(const variant_axis&) noexcept = default;
  variant_axis& operator=(variant_axis&&) noexcept = default;
  ~variant_axis() = default;

  template <typename... T>
  variant_axis(const T&... args)
  noexcept(std::is_nothrow_constructible_v<std::variant<Axes...>,const T&...>)
  : ax(std::forward<T>(args)...) { }

  index_type nbins() const noexcept {
    return std::visit([](auto& ax){ return ax.nbins(); }, ax);
  }
  index_type ndiv() const noexcept {
    return std::visit([](auto& ax){ return ax.ndiv(); }, ax);
  }
  index_type nedges() const noexcept {
    return std::visit([](auto& ax){ return ax.nedges(); }, ax);
  }

  edge_type edge(index_type i) const noexcept {
    return std::visit([i](auto& ax){ return ax.edge(i); }, ax);
  }
  edge_type operator[](index_type i) const noexcept { return edge(i); }

  edge_type min() const noexcept {
    return std::visit([](auto& ax){ return ax.min(); }, ax);
  }
  edge_type max() const noexcept {
    return std::visit([](auto& ax){ return ax.max(); }, ax);
  }

  edge_type lower(index_type i) const noexcept {
    return std::visit([i](auto& ax){ return ax.lower(i); }, ax);
  }
  edge_type upper(index_type i) const noexcept {
    return std::visit([i](auto& ax){ return ax.upper(i); }, ax);
  }

  index_type find_bin_index(edge_type x) const noexcept {
    return std::visit([x](auto& ax){ return ax.find_bin_index(x); }, ax);
  }
  index_type operator()(edge_type x) const noexcept {
    return find_bin_index(x);
  }

  const auto& operator*() const noexcept { return ax; }
  auto& operator*() noexcept { return ax; }
};

// ==================================================================

template <typename Axis>
[[nodiscard, gnu::const, gnu::always_inline]]
inline auto& get_axis_ref(Axis& a) {
  if constexpr (requires { a.nbins(); })
    return a;
  else
    return get_axis_ref(*a);
}

template <typename Axis>
using axis_edge_type = typename std::remove_reference_t<
    decltype(get_axis_ref(std::declval<Axis&>()))
  >::edge_type;

} // end namespace ivanp::hist

#endif
