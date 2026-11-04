//type:fp
//options:--c++20 -DHIDDEN=0:--c++20 --g++ --gnu_version=130300 -DHIDDEN=1:--c++20 --g++ --gnu_version=150200 -DHIDDEN=1:--c++20 --g++ --gnu_version=160100 -DHIDDEN=0:--c++20 --clang --clang_version=160000 -DHIDDEN=1:--c++20 --clang --clang_version=170000 -DHIDDEN=0:--ms_c++20 --microsoft_version=1944 -DHIDDEN=1:--ms_c++20 --microsoft_version=1950 -DHIDDEN=0:--ms_c++17 --microsoft_version=1950 -DHIDDEN=1 -DGHIDDEN=0 -DCPP17
//remark:visibility of init-captures in lambda headers in emulation modes

// P2579 makes init-captures visible throughout the lambda header (and not
// just in the lambda body).  GCC before version 16, Clang before version 17,
// and MSVC before version 19.50 (or in pre-C++20 modes) do not implement
// that: In the lambda header, a name of an init-capture refers to an entity of
// the enclosing scope instead.  The front end previously made init-captures
// visible in lambda headers in all modes.  HIDDEN is 1 for the modes in which
// init-captures are not visible in lambda headers.  GHIDDEN is the same for
// the exception specification and trailing return type of generic lambdas,
// except that in pre-C++20 modes, MSVC makes init-captures visible there.

#ifndef GHIDDEN
#define GHIDDEN HIDDEN
#endif

template <typename T, typename U> constexpr bool is_same = false;
template <typename T> constexpr bool is_same<T, T> = true;

struct X {
  int value, other;
};

// The type of "x" in the header of a lambda with the init-capture "x = 10"
// when there is also a variable "X x" in the enclosing scope.
#if HIDDEN
using HdrT = X;
#else
using HdrT = int;
#endif
#if GHIDDEN
using GenHdrT = X;
#else
using GenHdrT = int;
#endif

#define UNUSED [[maybe_unused]]

void f() {
  UNUSED X x = {2, 3};
  UNUSED auto l1 = [x = 10]() noexcept(is_same<decltype(x), X>) {
    return x;
  };
  static_assert(noexcept(l1()) == HIDDEN);
  UNUSED auto l2 = [x = 10]() -> decltype(x) { return {}; };
  static_assert(is_same<decltype(l2()), HdrT>);
  UNUSED auto l3 = [x = 10](decltype(x) p) { return p; };
  static_assert(is_same<decltype(l3(HdrT{})), HdrT>);
#if HIDDEN
  UNUSED auto l4 = [x = 10]() -> decltype((x)) { static X r{}; return r; };
  static_assert(is_same<decltype(l4()), X&>);
#endif
  // The init-capture is visible in the lambda body in all modes.
  UNUSED auto l5 = [x = 10]() { return x; };
  static_assert(is_same<decltype(l5()), int>);
  // Generic lambdas.
  UNUSED auto l6 = [x = 10](auto) noexcept(sizeof(x) == sizeof(X)) {
    return x;
  };
  static_assert(noexcept(l6(1)) == GHIDDEN);
  UNUSED auto l7 = [x = 10](auto, decltype(x) p) { return p; };
  static_assert(is_same<decltype(l7(1, HdrT{})), HdrT>);
  UNUSED auto l8 = [x = 10](auto) -> decltype(x) { return {}; };
  static_assert(is_same<decltype(l8(1)), GenHdrT>);
#ifndef CPP17
  UNUSED auto l9 = [x = 10]<class T>(T) -> decltype(x) { return {}; };
  static_assert(is_same<decltype(l9(1)), HdrT>);
#endif
}

void no_enclosing_variable() {
#if !HIDDEN
  UNUSED auto l = [y = 10]() -> decltype(y) { return y; };
  static_assert(is_same<decltype(l()), int>);
#endif
}

template <class T> void in_template() {
  UNUSED X x = {2, 3};
  UNUSED auto l1 = [x = T(10)]() noexcept(is_same<decltype(x), X>) {
    return x;
  };
  static_assert(noexcept(l1()) == HIDDEN);
  UNUSED auto l2 = [x = T(10)](auto) -> decltype(x) { return {}; };
  static_assert(is_same<decltype(l2(1)), GenHdrT>);
}
template void in_template<int>();

#if HIDDEN && !defined(CPP17)
template <class... T> void init_capture_pack(T... t) {
  UNUSED X x = {2, 3};
  UNUSED auto l = [...x = t]() noexcept(is_same<decltype(x), X>) {
    return sizeof...(x);
  };
  static_assert(noexcept(l()));
}
template void init_capture_pack(int, int);
#endif
