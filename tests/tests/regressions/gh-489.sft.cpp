//type:fp
//options:--g++ --c++03 --gnu_version=150200:--clang --c++03 --clang_version=220100:--g++ --c++11 --gnu_version=150200
//remark:Template-dependent variable template-ids in C++03 constant expressions

// In C++03 GNU and Clang modes (where variable templates are an extension),
// a variable template-id with template-dependent arguments was rejected as
// nonconstant where a constant expression is required.

template <bool B> struct integral_constant {
  static const bool value = B;
};

template <class T> inline const bool is_fp_impl              = false;
template <>        inline const bool is_fp_impl<float>       = true;
template <>        inline const bool is_fp_impl<double>      = true;
template <>        inline const bool is_fp_impl<long double> = true;

template <class T>
struct is_floating_point : integral_constant<is_fp_impl<T> > {};

int a1[is_floating_point<float>::value ? 1 : -1];
int a2[is_floating_point<int>::value ? -1 : 1];

// Value-dependent template argument; other constant-expression contexts.
template <int N> const bool positive = N > 0;
template <int N> struct X : integral_constant<positive<N> > {
  int arr[positive<N> ? 1 : 2];
  enum { e = positive<N> };
};
int b1[X<1>::value ? 1 : -1];
int b2[sizeof(X<1>().arr) == sizeof(int) ? 1 : -1];
int b3[X<0>::e == 0 ? 1 : -1];

// Member variable template.
struct S {
  template <class T> static const bool v = false;
};
template <> const bool S::v<float> = true;
template <class T> struct Y : integral_constant<S::v<T> > {};
int c1[Y<float>::value ? 1 : -1];
int c2[Y<int>::value ? -1 : 1];
