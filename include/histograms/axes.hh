#ifndef IVANP_HISTOGRAMS_AXES_HH
#define IVANP_HISTOGRAMS_AXES_HH

#include <type_traits>
#include <limits>
#include <algorithm>
#include <iterator>

namespace histograms {

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

  virtual ~poly_axis_base() = 0;
  virtual index_type nedges() const = 0;
  virtual index_type nbins() const = 0;

  virtual index_type find_bin(edge_type x) const = 0;

  virtual edge_type edge(index_type i) const = 0;
  virtual edge_type min() const = 0;
  virtual edge_type max() const = 0;
  virtual edge_type lower(index_type i) const = 0;
  virtual edge_type upper(index_type i) const = 0;
};

template <bool Poly, typename Edge>
using axis_base_t = std::conditional_t< Poly,
  poly_axis_base<Edge>, axis_base<Edge> >;

// Container axis ===================================================

template <typename Container, bool Poly=false>
class container_axis final: public axis_base_t< Poly,
  typename std::decay_t<Container>::value_type >
{
public:
  using base_type = axis_base_t<
    Poly, typename std::decay_t<Container>::value_type >;
  using edge_type = typename base_type::edge_type;
  using container_type = Container;

  static constexpr edge_type lowest = axis_base<edge_type>::lowest;
  static constexpr edge_type highest = axis_base<edge_type>::lowest;

private:
  container_type _edges;

public:
  container_axis() { }
  container_axis(const container_axis&) = default;
  container_axis(container_axis&&) = default;
  container_axis& operator=(const container_axis&) = default;
  container_axis& operator=(container_axis&&) = default;

  container_axis(const container_type& edges): _edges(edges) { }
  container_axis(container_type&& edges): _edges(std::move(edges)) { }
  container_axis& operator=(const container_type& edges) {
    _edges = edges;
    return *this;
  }
  container_axis& operator=(container_type&& edges) {
    _edges = std::move(edges);
    return *this;
  }

  container_axis(std::initializer_list<edge_type> edges): _edges(edges) { }
  container_axis& operator=(std::initializer_list<edge_type> edges) {
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

  template <typename T>
  index_type find_bin(const T& x) const noexcept {
    using namespace std;
    return distance(
      begin(_edges), upper_bound(begin(_edges), end(_edges), x)
    );
  }
  template <typename T>
  index_type operator()(const T& x) const noexcept { return find_bin(x); }

  const container_type& edges() const { return _edges; }
};

// Uniform axis =====================================================

template <typename EdgeT, bool Poly=false>
class uniform_axis final: public axis_base_t< Poly, EdgeT >
{
public:
  using base_type = axis_base_t< Poly, EdgeT >;
  using edge_type = typename base_type::edge_type;

  static constexpr edge_type lowest = axis_base<edge_type>::lowest;
  static constexpr edge_type highest = axis_base<edge_type>::lowest;

private:
  index_type _nbins;
  edge_type _min, _max;

public:
  uniform_axis() { }
  uniform_axis(const uniform_axis&) = default;
  uniform_axis(uniform_axis&&) = default;
  uniform_axis& operator=(const uniform_axis&) = default;
  uniform_axis& operator=(uniform_axis&&) = default;

  uniform_axis(index_type nbins, edge_type min, edge_type max)
  : _nbins(nbins), _min(std::min(min,max)), _max(std::max(min,max)) { }

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

  template <typename T>
  index_type find_bin(const T& x) const noexcept {
    if (x < _min) return 0;
    if (!(x < _max)) return _nbins+1;
    return index_type(_nbins*(x-_min)/(_max-_min)) + 1;
  }
  template <typename T>
  index_type operator()(const T& x) const noexcept { return find_bin(x); }
};

// Reference axis ===================================================

template <typename AxisT, bool Poly=false>
class ref_axis final: public axis_base_t< Poly, typename AxisT::edge_type >
{
public:
  using base_type = axis_base_t< Poly, typename AxisT::value_type >;
  using edge_type = typename base_type::edge_type;

private:
  const AxisT* _ref;

public:
  ref_axis(const ref_axis&) = default;
  ref_axis& operator=(const ref_axis&) = default;

  ref_axis(const AxisT& a): _ref(&a) { }

  index_type nedges() const noexcept { return _ref->nedges(); }
  index_type nbins() const noexcept { return _ref->nbins(); }

  edge_type edge(index_type i) const noexcept { return _ref->edge(i); }
  edge_type operator[](index_type i) const noexcept { return edge(i); }

  edge_type min() const noexcept { return _ref->min(); }
  edge_type max() const noexcept { return _ref->max(); }

  edge_type lower(index_type i) const noexcept { return _ref->lower(i); }
  edge_type upper(index_type i) const noexcept { return _ref->upper(i); }

  template <typename T>
  index_type find_bin(const T& x) const noexcept { return _ref->find_bin(x); }
  template <typename T>
  index_type operator()(const T& x) const noexcept { return find_bin(x); }
};

} // end namespace histograms

#endif
