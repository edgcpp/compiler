//type:fp
//options:--c++20

  template<typename T> struct S { S(T); };
  template<template<typename> class TT> using A = decltype(TT(0));
  template<template<typename> class TT>
  constexpr bool b = requires { typename A<TT>; };
  static_assert(b<S>);  // Previously an internal error, now okay.
