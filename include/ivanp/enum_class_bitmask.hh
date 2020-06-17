#ifndef IVANP_ENUM_CLASS_BITMASK_HH
#define IVANP_ENUM_CLASS_BITMASK_HH

namespace ivanp {

template <typename Enum>
constexpr bool enable_bitmask_operators = false;

template <typename Enum>
concept BitMask = enable_bitmask_operators<Enum>;

}

#define IVANP_UNARY_BITMASK_OPERATOR(OP) \
  template <ivanp::BitMask Enum> \
  constexpr Enum operator OP(Enum rhs) noexcept { \
    using type = std::underlying_type_t<Enum>; \
    return static_cast<Enum>( OP static_cast<type>(rhs) ); \
  }

#define IVANP_BINARY_BITMASK_OPERATOR(OP) \
  template <ivanp::BitMask Enum> \
  constexpr Enum operator OP(Enum lhs, Enum rhs) noexcept { \
    using type = std::underlying_type_t<Enum>; \
    return static_cast<Enum>( \
      static_cast<type>(lhs) OP \
      static_cast<type>(rhs) \
    ); \
  }

#define IVANP_ASSIGNMENT_BITMASK_OPERATOR(OP) \
  template <ivanp::BitMask Enum> \
  constexpr Enum& operator OP(Enum& lhs, Enum rhs) noexcept { \
    using type = std::underlying_type_t<Enum>; \
    return static_cast<Enum&>( \
      static_cast<type&>(lhs) OP \
      static_cast<type>(rhs) \
    ); \
  }

IVANP_UNARY_BITMASK_OPERATOR(~)

IVANP_BINARY_BITMASK_OPERATOR(|)
IVANP_BINARY_BITMASK_OPERATOR(&)
IVANP_BINARY_BITMASK_OPERATOR(^)

IVANP_ASSIGNMENT_BITMASK_OPERATOR(|=)
IVANP_ASSIGNMENT_BITMASK_OPERATOR(&=)
IVANP_ASSIGNMENT_BITMASK_OPERATOR(^=)

template <ivanp::BitMask Enum>
constexpr bool operator!(Enum rhs) noexcept {
  using type = std::underlying_type_t<Enum>;
  return !static_cast<type>(rhs);
}

#endif
