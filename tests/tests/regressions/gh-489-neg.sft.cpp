//type:fn
//options:--g++ --c++03 --gnu_version=150200:--clang --c++03 --clang_version=220100:--g++ --c++11 --gnu_version=150200
//remark:Non-constant variable template-id rejected in a constant expression

// A variable template that is not const is still not usable where a constant
// expression is required.  The error is issued when the template is
// instantiated.

template <bool B> struct integral_constant {
  static const bool value = B;
};

template <class T> bool nonconst = false;
template <class T> struct Z : integral_constant<nonconst<T> > {};
Z<int> z;
