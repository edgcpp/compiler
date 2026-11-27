//type:cp
//options:--c++17:--c++17 --gnu_version 150200:--c++17 --clang_version 230100:--ms_c++17 --microsoft_version 1950
//remark:Reconstruct a cast whose deduced type involves an unnamed lambda

template<typename T> T&& declval();

template<typename T> struct identity {
  using type = T;
};

template<typename F> struct invoke_result {
  template<typename G>
  static identity<decltype(declval<G>()(0))> test(int);
  using type = typename decltype(test<F>(0))::type;
};

template<typename F> using invoke_result_t = typename invoke_result<F>::type;

template<typename T> struct result {
  T value;
};

template<typename F> result<invoke_result_t<F>> transform(int input, F&& f) {
  return {f(input)};
}

int f(int input) {
  // Passing the lambda directly leaves its closure type without a name in
  // the generated C++.  Preserve decltype(r)&& through reference collapsing
  // so the cast can refer to r without spelling out invoke_result_t<F>.
  auto&& r = transform(input, [](auto x) { return x + 1; });
  return static_cast<decltype(r)&&>(r).value;
}

auto make_callable() {
  struct local {};
  return [](auto) { return local{}; };
}

auto callable = make_callable();
// Checking the inner decltype must preserve the member-access exemption
// for the call.  The outer decltype is usable through callable, and its
// resolved type local cannot be named here.
decltype(callable.operator()<decltype(0)>(0)) value;
