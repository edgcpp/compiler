[[gnu::always_inline]] inline int twice(int x) { return x + x; }

// Like std::move and std::forward in libstdc++ 15.
template<typename T> [[gnu::always_inline]] constexpr T &&move_it(T &t) {
  return static_cast<T &&>(t);
}
