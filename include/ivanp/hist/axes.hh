#ifndef IVANP_HISTOGRAMS_AXES_HH
#define IVANP_HISTOGRAMS_AXES_HH

#include <type_traits>
#include <limits>
#include <algorithm>
#include <iterator>
#include <variant>

#include <ivanp/cont/general.hh>
#include <ivanp/cont/map.hh>

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
  using init_list = std::initializer_list<edge_type>;

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

  cont_axis(const cont_type& edges)
  noexcept(std::is_nothrow_copy_constructible_v<cont_type>)
  : _edges(edges) { }
  cont_axis(cont_type&& edges)
  noexcept(std::is_nothrow_move_constructible_v<cont_type>)
  : _edges(std::move(edges)) { }
  cont_axis& operator=(const cont_type& edges)
  noexcept(std::is_nothrow_copy_assignable_v<cont_type>) {
    _edges = edges;
    return *this;
  }
  cont_axis& operator=(cont_type&& edges)
  noexcept(std::is_nothrow_move_assignable_v<cont_type>) {
    _edges = std::move(edges);
    return *this;
  }

  cont_axis(init_list edges)
  noexcept(std::is_nothrow_constructible_v< cont_type, init_list >)
  : _edges(edges) { }
  cont_axis& operator=(init_list edges)
  noexcept(std::is_nothrow_assignable_v< cont_type, init_list >) {
    _edges = edges;
    return *this;
  }

  template <cont::Container C>
  requires(requires { (cont_type)std::declval<C&&>(); })
  cont_axis(C&& edges): cont_axis((cont_type)std::forward<C>(edges)) { }

  template <cont::Container C>
  cont_axis(C&& edges) {
    if constexpr (cont::Tuple<C>) {
      cont::map<cont::map_flags::forward>(
        []<typename A, typename B>(A& a, B&& b){
          a = (A)std::forward<B>(b);
        }, _edges, edges);
    } else {
      // TODO: non-stl case
      if constexpr (requires { _edges.reserve(size_t{}); })
        _edges.reserve(cont::size(edges));
      cont::map<cont::map_flags::forward>([&]<typename A>(A&& a){
        if constexpr (requires { _edges.emplace_back(std::forward<A>(a)); }) {
          _edges.emplace_back(std::forward<A>(a));
        } else {
          _edges.emplace(std::forward<A>(a));
        }
      }, edges);
    }
  }

  index_type nbins () const noexcept { return cont::size(_edges)+1; }
  index_type ndiv  () const noexcept { return cont::size(_edges)-1; }
  index_type nedges() const noexcept { return cont::size(_edges)  ; }

  edge_type edge(index_type i) const noexcept { return cont::at(_edges,i); }
  edge_type operator[](index_type i) const noexcept { return edge(i); }

  edge_type min() const { return cont::first(_edges); }
  edge_type max() const { return cont::last (_edges); }

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
  edge_type _min, _max;
  index_type _ndiv;

public:
  constexpr uniform_axis() noexcept = default;
  constexpr uniform_axis(const uniform_axis&) noexcept = default;
  constexpr uniform_axis(uniform_axis&&) noexcept = default;
  constexpr uniform_axis& operator=(const uniform_axis&) noexcept = default;
  constexpr uniform_axis& operator=(uniform_axis&&) noexcept = default;
  ~uniform_axis() = default;

  constexpr uniform_axis(edge_type min, edge_type max, index_type ndiv)
  noexcept: _min(min), _max(max), _ndiv(ndiv)
  {
    if (_max < _min) std::swap(_min,_max);
  }

  constexpr index_type nbins () const noexcept { return _ndiv+2; }
  constexpr index_type ndiv  () const noexcept { return _ndiv  ; }
  constexpr index_type nedges() const noexcept { return _ndiv+1; }

  constexpr edge_type edge(index_type i) const noexcept {
    return _min + i*((_max - _min)/_ndiv);
  }
  constexpr edge_type operator[](index_type i) const noexcept
  { return edge(i); }

  constexpr edge_type min() const noexcept { return _min; }
  constexpr edge_type max() const noexcept { return _max; }

  constexpr edge_type lower(index_type i) const noexcept {
    if (i==0) return lowest;
    if (i > _ndiv+1) return highest;
    return edge(i-1);
  }
  constexpr edge_type upper(index_type i) const noexcept {
    if (i > _ndiv) return highest;
    return edge(i);
  }

  constexpr index_type find_bin_index(edge_type x) const noexcept {
    if (x < _min) return 0;
    if (!(x < _max)) return _ndiv+1;
    return index_type(_ndiv*(x-_min)/(_max-_min)) + 1;
  }
  constexpr index_type operator()(edge_type x) const noexcept {
    return find_bin_index(x);
  }
};

// Variant axis =====================================================
template <typename... Axes>
class variant_axis {
public:
  using edge_type = std::common_type_t<typename Axes::edge_type...>;
  using variant_type = std::variant<Axes...>;

private:
  variant_type ax;

public:
  variant_axis() = default;
  variant_axis(const variant_axis&) = default;
  variant_axis(variant_axis&&) = default;
  variant_axis& operator=(const variant_axis&) = default;
  variant_axis& operator=(variant_axis&&) = default;
  ~variant_axis() = default;

  template <typename... Args>
  variant_axis(Args&&... args)
  noexcept(std::is_nothrow_constructible_v<variant_type,Args&&...>)
  : ax(std::forward<Args>(args)...) { }

  index_type nbins() const {
    return std::visit([](auto& ax){ return ax.nbins(); }, ax);
  }
  index_type ndiv() const {
    return std::visit([](auto& ax){ return ax.ndiv(); }, ax);
  }
  index_type nedges() const {
    return std::visit([](auto& ax){ return ax.nedges(); }, ax);
  }

  decltype(auto) edge(index_type i) const {
    return std::visit([i](auto& ax){ return ax.edge(i); }, ax);
  }
  decltype(auto) operator[](index_type i) const { return edge(i); }

  decltype(auto) min() const {
    return std::visit([](auto& ax){ return ax.min(); }, ax);
  }
  decltype(auto) max() const {
    return std::visit([](auto& ax){ return ax.max(); }, ax);
  }

  decltype(auto) lower(index_type i) const {
    return std::visit([i](auto& ax){ return ax.lower(i); }, ax);
  }
  decltype(auto) upper(index_type i) const {
    return std::visit([i](auto& ax){ return ax.upper(i); }, ax);
  }

  index_type find_bin_index(const auto& x) const {
    return std::visit([x](auto& ax){ return ax.find_bin_index(x); }, ax);
  }
  index_type operator()(const auto& x) const {
    return find_bin_index(x);
  }

  const auto& operator*() const { return ax; }
  auto& operator*() { return ax; }
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
