#ifndef IVANP_CONCEPTS_HH
#define IVANP_CONCEPTS_HH

namespace ivanp {

template <typename From, typename To>
concept convertible_to = std::is_convertible_v<From, To>;

}

#endif
