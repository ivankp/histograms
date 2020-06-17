#ifndef IVANP_MAP_HH
#define IVANP_MAP_HH

#include <functional>
#include <array>
#include <vector>
#include <stdexcept>

#include <ivanp/map/concepts.hh>
#include <ivanp/enum_class_bitmask.hh>

namespace ivanp::map {
enum class flags {
  none = 0,
  forward = 1 << 0,
  no_return = 1 << 1,
  no_static_size_check  = 1 << 2,
  no_dynamic_size_check = 1 << 3,
  no_size_check = no_static_size_check | no_dynamic_size_check,
  prefer_tuple  = 1 << 4,
  prefer_iteration = 1 << 5
};
}

namespace ivanp {
template <>
constexpr bool enable_bitmask_operators<map::flags> = true;
}

namespace ivanp::map {
namespace impl {

template <typename C>
struct iterator_wrapper {
  struct tuple {
    C&& ref;
    static constexpr bool is_tuple = true;
  };
  struct list {
    std::decay_t<decltype(std::begin(std::declval<C&>()))> first;
    std::decay_t<decltype(std::end  (std::declval<C&>()))> second;
    bool operator!() const noexcept { return first == second; }
    static constexpr bool is_tuple = false;
    static constexpr bool has_size = Sizable<C&>;
  };
};

template <flags flags, typename F, typename... C>
inline decltype(auto) map(F&& f, C&&... c) {
  using indices = container_index_sequence<C...>;
  using dimensions = std::index_sequence_for<C...>;

  static constexpr bool got_tuples = (... || Tuple<C>);
  static constexpr bool map_by_unfolding =
    got_tuples &&
    !(!!(flags & flags::prefer_iteration) && (... && Iterable<C>));

  if constexpr (sizeof...(C) > 1) {
    if constexpr (!(flags & flags::no_static_size_check)) {
      (..., []<typename _C>(type_constant<_C>) {
        static_assert(
          !Tuple<_C> || indices::size() == container_size<_C>,
          "tuples of unequal size given to map");
      }(type_constant<C>{}));
    }
    if constexpr (!(flags & flags::no_dynamic_size_check)) {
      auto impl = [
        first = !got_tuples,
        s = indices::size()
      ] <typename _C> (_C&& _c) mutable {
        if constexpr (
          Sizable<_C> && !(!(flags & flags::no_static_size_check) && Tuple<_C>)
        ) {
          if (first) {
            first = false;
            s = std::size(_c);
          } else if (std::size(_c) != s)
            throw std::length_error("containers of unequal size given to map");
        }
      };
      (..., impl(c));
    }
  }

  using result_types =
    decltype([]<size_t... I>(std::index_sequence<I...>) {
      auto impl = []<size_t J>(index_constant<J>) {
        return type_constant<
          std::invoke_result_t<F&&,container_element_t<C&&,J>...>
        >{};
      };
      return type_sequence<typename decltype(
        impl(index_constant<I>{}) )::type ...>{};
    }(indices{}));

  static constexpr auto ret =
    []<typename... T>(type_sequence<T...>) {
      struct ret_t { bool no_return, same, refs, constructible; };
      return ret_t {
        ((!!(flags & flags::no_return)) || ... || std::is_void_v<T>),
        are_same_v<T...>,
        (... || std::is_reference_v<T>),
        (... || std::is_constructible_v<std::decay_t<T>,T>),
      };
    }(result_types{});

  auto iterators = std::make_tuple(
    []<typename _C>(_C&& _c) {
      if constexpr (map_by_unfolding && Tuple<_C>)
        return typename iterator_wrapper<_C>::tuple
        { std::forward<_C>(_c) };
      else
        return typename iterator_wrapper<_C>::list
        { std::begin(_c), std::end(_c) };
    }(std::forward<C>(c)) ...
  );

  if constexpr (map_by_unfolding) { // map to tuple
    // Note: evaluation order is sequential for list-initialization
    // https://en.cppreference.com/w/cpp/language/eval_order   Rule 10
    return [&]<size_t... I>(std::index_sequence<I...>) {
      auto impl = [&]<size_t J>(index_constant<J>) -> decltype(auto) {
        return [&]<size_t... Ks>(std::index_sequence<Ks...>) -> decltype(auto) {
          return std::invoke(
            std::forward<F>(f),
            [&]<typename Iter>(Iter&& iter) -> decltype(auto) {
            using iter_t = std::remove_reference_t<Iter>;
              if constexpr (iter_t::is_tuple) {
                return std::get<J>(iter.ref);
              } else {
                auto& it = iter.first;
                if constexpr (
                  sizeof...(C)>1
                  && !(flags & flags::no_dynamic_size_check)
                  && !iter_t::has_size
                ) {
                  if (it == iter.end) throw std::length_error(
                    "in map: iterable ended before tuples");
                }
                decltype(auto) x = *it;
                ++it;
                return x;
              }
            }(std::get<Ks>(iterators)) ...
          );
        }(dimensions{});
      };

      if constexpr ( ret.no_return )
        ( ..., impl(index_constant<I>{}) );
      else if constexpr (
        !(flags & flags::prefer_tuple) &&
        ret.constructible &&
        ret.same && ( !(flags & flags::forward) || !ret.refs )
      )
        return std::array { impl(index_constant<I>{}) ... };
      else if constexpr ( !(flags & flags::forward) && ret.constructible )
        return std::tuple { impl(index_constant<I>{}) ... };
      else {
        // can't use std::forward_as_tuple() because the order of evaluation
        // of function arguments is not specified
        using return_type = decltype(
          []<typename... T>(type_sequence<T...>) -> std::tuple<T&&...> {
            return { };
          }(result_types{}));
        return return_type{ impl(index_constant<I>{}) ... };
      }
    }(indices{});
  } else { // map to vector
    auto not_done = [&]<size_t... K>(index_constant<K>...) -> bool {
      if constexpr (
        !!(flags & flags::no_dynamic_size_check)
        || sizeof...(K) == 1
        || (... && Sizable<C&>) // already checked
      ) {
        if ((... || ( !std::get<K>(iterators) ))) return false;
      } else {
        const size_t n_ended = (... + ( !std::get<K>(iterators) ));
        if (n_ended == sizeof...(K)) return false;
        else if (n_ended != 0) throw std::length_error(
          "in map: container reached end before others");
      }
      return true;
    };
    return [&]<size_t... K>(std::index_sequence<K...>) -> decltype(auto) {
      if constexpr ( ret.no_return ) {
        while (not_done(index_constant<K>{}...)) {
          std::invoke(
            std::forward<F>(f), *std::get<K>(iterators).first ... );
          ( ..., ++std::get<K>(iterators).first );
        }
      } else {
        // must create the vector with a lambda here
        // because of a bug in GCC 10.1.0
        auto out = []<typename T, typename... Ts>(type_sequence<T,Ts...>) ->
            std::vector<
              std::conditional_t<
                !(flags & flags::forward)
                || !std::is_lvalue_reference_v<T>,
                std::decay_t<T>,
                std::reference_wrapper<std::remove_reference_t<T>>
              >
            >
          { return { }; }(result_types{});

        ( ... || [&]<typename _C>(_C&& c){
          if constexpr (Sizable<_C>) {
            out.reserve(std::size(c));
            return true;
          } else return false;
        }(c) );

        while (not_done(index_constant<K>{}...)) {
          out.push_back( std::invoke(
            std::forward<F>(f), *std::get<K>(iterators).first ... ) );
          ( ..., ++std::get<K>(iterators).first );
        }
        return out;
      }
    }(dimensions{});
  }
}

} // end namespace impl

template <flags flags=flags::none, Container... C, typename F>
requires InvocableForElements<F&&,C&&...>
inline decltype(auto) map(F&& f, C&&... c) {
  return impl::map<flags>(std::forward<F>(f),std::forward<C>(c)...);
}

template <flags flags=flags::none, typename... T, typename F>
requires Invocable<F&&,T...>
inline decltype(auto) map(F&& f, std::initializer_list<T>... c) {
  return impl::map<flags>(std::forward<F>(f),c...);
}

template <flags flags=flags::none, typename F>
requires Invocable<F&&>
inline decltype(auto) map(F&& /*f*/) {
  // return impl::map<flags>(std::forward<F>(f));
}

namespace operators { // --------------------------------------------

template <Container C, typename F>
requires InvocableForElements<F&&,C&&>
inline decltype(auto) operator|(C&& c, F&& f) {
  return impl::map<flags::none>(std::forward<F>(f),std::forward<C>(c));
}

template <Container C, typename F>
requires InvocableForElements<F&&,C&&>
inline decltype(auto) operator||(C&& c, F&& f) {
  return impl::map<flags::forward>(std::forward<F>(f),std::forward<C>(c));
}

template <Tuple C, typename F>
requires Applyable<F&&,C&&>
inline constexpr decltype(auto) operator%(C&& c, F&& f) {
  return std::apply(std::forward<F>(f),std::forward<C>(c));
}

} // end namespace operators
} // end namespace map

#endif
