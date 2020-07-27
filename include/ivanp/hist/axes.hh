#ifndef IVANP_HISTOGRAMS_AXES_HH
#define IVANP_HISTOGRAMS_AXES_HH

#include <type_traits>
#include <limits>
#include <algorithm>
#include <iterator>
#include <ivanp/concepts.hh>

namespace ivanp::hist {

using index_type = unsigned;

template <typename Edge>
struct axis_base {
  using edge_type = Edge;

  static constexpr edge_type lowest =
    std::numeric_limits<edge_type>::has_infinity
    ? -std::numeric_limits<edge_type>::infinity()
    : std::numeric_limits<edge_type>::lowest();

  static constexpr edge_type highest =
    std::numeric_limits<edge_type>::has_infinity
    ? std::numeric_limits<edge_type>::infinity()
    : std::numeric_limits<edge_type>::max();
};

template <typename Edge>
struct poly_axis_base: axis_base<Edge> {
  using edge_type = typename axis_base<Edge>::edge_type;

  virtual ~poly_axis_base() { }
  virtual index_type nedges() const = 0;
  virtual index_type nbins() const = 0;

  virtual index_type find_bin_index(edge_type x) const = 0;

  virtual edge_type edge(index_type i) const = 0;
  virtual edge_type min() const = 0;
  virtual edge_type max() const = 0;
  virtual edge_type lower(index_type i) const = 0;
  virtual edge_type upper(index_type i) const = 0;
};

template <typename Edge, bool Poly>
using axis_base_t = std::conditional_t<
  Poly, poly_axis_base<Edge>, axis_base<Edge> >;

template <typename A, typename Edge=void>
concept Axis = requires (A a) {
  { a.nedges()                             } -> convertible_to<index_type>;
  { a.nbins()                              } -> convertible_to<index_type>;
  { a.find_bin_index(std::declval<Edge>()) } -> convertible_to<index_type>;
  { a.edge(std::declval<index_type>())     } -> convertible_to_or_any<Edge>;
  { a.min()                                } -> convertible_to_or_any<Edge>;
  { a.max()                                } -> convertible_to_or_any<Edge>;
  { a.lower(std::declval<index_type>())    } -> convertible_to_or_any<Edge>;
  { a.upper(std::declval<index_type>())    } -> convertible_to_or_any<Edge>;
  typename A::edge_type;
};

// List axis ===================================================

template <
  typename List = std::vector<double>,
  bool Poly = false,
  typename Edge = typename std::remove_reference_t<List>::value_type
>
class list_axis final: public axis_base_t<Edge,Poly>
{
public:
  using base_type = axis_base_t<Edge,Poly>;
  using edge_type = Edge;
  using list_type = List;

  static constexpr edge_type lowest = axis_base<edge_type>::lowest;
  static constexpr edge_type highest = axis_base<edge_type>::lowest;

private:
  list_type _edges;

public:
  list_axis() noexcept = default;
  list_axis(const list_axis&) noexcept = default;
  list_axis(list_axis&&) noexcept = default;
  list_axis& operator=(const list_axis&) noexcept = default;
  list_axis& operator=(list_axis&&) noexcept = default;
  ~list_axis() = default;

  list_axis(const list_type& edges): _edges(edges) { }
  list_axis(list_type&& edges)
  noexcept(std::is_nothrow_move_constructible_v<list_type>)
  : _edges(std::move(edges)) { }
  list_axis& operator=(const list_type& edges) {
    _edges = edges;
    return *this;
  }
  list_axis& operator=(list_type&& edges)
  noexcept(std::is_nothrow_move_assignable_v<list_type>) {
    _edges = std::move(edges);
    return *this;
  }

  list_axis(std::initializer_list<edge_type> edges): _edges(edges) { }
  list_axis& operator=(std::initializer_list<edge_type> edges) {
    _edges = edges;
    return *this;
  }

  index_type nedges() const noexcept { return _edges.size(); }
  index_type nbins() const noexcept { return _edges.size()-1; }

  edge_type edge(index_type i) const noexcept { return _edges[i]; }
  edge_type operator[](index_type i) const noexcept { return edge(i); }

  edge_type min() const noexcept(noexcept(_edges.front()))
  { return _edges.front(); }
  edge_type max() const noexcept(noexcept(_edges.back()))
  { return _edges.back(); }

  edge_type lower(index_type i) const noexcept {
    return i==0 ? lowest : i>nedges()+1 ? highest : edge(i-1);
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

  list_type& edges() noexcept { return _edges; }
  const list_type& edges() const noexcept { return _edges; }
};

// Uniform axis =====================================================

template <typename Edge = double, bool Poly = false>
class uniform_axis final: public axis_base_t<Edge,Poly>
{
public:
  using base_type = axis_base_t<Edge,Poly>;
  using edge_type = Edge;

  static constexpr edge_type lowest = axis_base<edge_type>::lowest;
  static constexpr edge_type highest = axis_base<edge_type>::lowest;

private:
  index_type _nbins;
  edge_type _min, _max;

public:
  uniform_axis() noexcept = default;
  uniform_axis(const uniform_axis&) noexcept = default;
  uniform_axis(uniform_axis&&) noexcept = default;
  uniform_axis& operator=(const uniform_axis&) noexcept = default;
  uniform_axis& operator=(uniform_axis&&) noexcept = default;
  ~uniform_axis() = default;

  uniform_axis(index_type nbins, edge_type min, edge_type max) noexcept
  : _nbins(nbins), _min(min), _max(max)
  {
    if (_max < _min) std::swap(_min,_max);
  }

  index_type nedges() const noexcept { return _nbins+1; }
  index_type nbins() const noexcept { return _nbins; }

  edge_type edge(index_type i) const noexcept {
    return _min + i*((_max - _min)/_nbins);
  }
  edge_type operator[](index_type i) const noexcept { return edge(i); }

  edge_type min() const noexcept { return _min; }
  edge_type max() const noexcept { return _max; }

  edge_type lower(index_type i) const noexcept {
    if (i==0) return lowest;
    if (i > _nbins+2) return highest;
    return edge(i-1);
  }
  edge_type upper(index_type i) const noexcept {
    if (i > _nbins) return highest;
    return edge(i);
  }

  index_type find_bin_index(edge_type x) const noexcept {
    if (x < _min) return 0;
    if (!(x < _max)) return _nbins+1;
    return index_type(_nbins*(x-_min)/(_max-_min)) + 1;
  }
  index_type operator()(edge_type x) const noexcept {
    return find_bin_index(x);
  }
};

} // end namespace ivanp::hist

#endif
