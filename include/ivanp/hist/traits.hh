#ifndef IVANP_TRAITS_HH
#define IVANP_TRAITS_HH

#include <type_traits>

namespace ivanp::hist {

template <typename T, typename...> struct pack_head { using type = T; };
template <typename... T> using head_t = typename pack_head<T...>::type;

template <typename...> using void_t = void;

template <typename AlwaysVoid,
          template<typename...> typename Op, typename... Args>
struct detector {
  static constexpr bool value = false;
  using type = void;
};

template <template<typename...> typename Op, typename... Args>
struct detector<void_t<Op<Args...>>, Op, Args...> {
  static constexpr bool value = true;
  using type = Op<Args...>;
};

// ==================================================================

#define IVANP_MAKE_OP_TRAIT_CLASS(NAME,EXPR) \
  class NAME { \
    template <typename T> \
    using expr_type = decltype(EXPR); \
    using detect = detector<void,expr_type,U>; \
  public: \
    static constexpr bool value = detect::value; \
    using type = typename detect::type; \
  };

#define IVANP_MAKE_OP_TRAIT_1(NAME,EXPR) \
  template <typename U> \
  IVANP_MAKE_OP_TRAIT_CLASS(NAME,EXPR)

#define IVANP_MAKE_OP_TRAIT_2(NAME,EXPR) \
  template <typename U, typename T2> \
  IVANP_MAKE_OP_TRAIT_CLASS(NAME,EXPR)

#define IVANP_MAKE_OP_TRAIT_PACK(NAME,EXPR) \
  template <typename U, typename... Args> \
  IVANP_MAKE_OP_TRAIT_CLASS(NAME,EXPR)

// ==================================================================

IVANP_MAKE_OP_TRAIT_PACK( is_callable, std::declval<T&>()(std::declval<Args>()...) )
IVANP_MAKE_OP_TRAIT_PACK( is_constructible, T(std::declval<Args>()...) )

IVANP_MAKE_OP_TRAIT_2( is_assignable, std::declval<T&>()=std::declval<T2>() )

IVANP_MAKE_OP_TRAIT_1( has_pre_increment,  ++std::declval<T&>() )
IVANP_MAKE_OP_TRAIT_1( has_post_increment, std::declval<T&>()++ )
IVANP_MAKE_OP_TRAIT_1( has_pre_decrement,  --std::declval<T&>() )
IVANP_MAKE_OP_TRAIT_1( has_post_decrement, std::declval<T&>()-- )

IVANP_MAKE_OP_TRAIT_2( has_plus_eq,  std::declval<T&>()+=std::declval<T2>() )
IVANP_MAKE_OP_TRAIT_2( has_minus_eq, std::declval<T&>()-=std::declval<T2>() )

IVANP_MAKE_OP_TRAIT_1( has_deref, *std::declval<T&>() )

// ==================================================================

} // end namespace

#endif
