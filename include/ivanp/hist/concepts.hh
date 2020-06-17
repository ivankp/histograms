#ifndef IVANP_HIST_CONCEPTS_HH
#define IVANP_HIST_CONCEPTS_HH

template <typename T, typename N=size_t>
concept Resizable = requires (T& x, N n) {
  x.resize(n);
};

#endif
