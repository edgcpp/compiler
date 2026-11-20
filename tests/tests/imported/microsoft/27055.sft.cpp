//type:cp
//options_all:--ms_c++20 --microsoft_v 1940 -w
template <class T> constexpr bool always_true = true;

template <class T, bool = always_true<T>> constexpr void func(T) {}

template <class U> constexpr int thing = 1;

template <class U> constexpr bool can_call_func_with_thing() {
  using ThingType = decltype(thing<U>);
  return requires(ThingType val) { ::func(val); };
}

static_assert(can_call_func_with_thing<double>());
