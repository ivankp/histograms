#ifndef IVANP_CONCEPTS_HH
#define IVANP_CONCEPTS_HH

namespace ivanp {

template <typename From, typename To>
concept convertible_to = std::is_convertible_v<From, To>;

template <typename T>
concept stringlike = requires (T& x) {
  { x.data() } -> convertible_to<const char*>;
  { x.size() } -> convertible_to<size_t>;
};

}

#endif
