//type:fp
//options:--c++20:--c++20 --g++ --gnu_version=150200:--c++20 --clang --clang_version=220100:--ms_c++20 --microsoft_version 1950

// Class template argument deduction through an alias template crashed when
// the noexcept specification of a constructor template called a member
// function template with the constructor's parameter pack as explicit
// template arguments, e.g. aliases of std::tuple with libstdc++.  The
// explicit template arguments of the call were substituted twice.

template<typename... E> struct S
{
  template<typename... U> static constexpr bool n() { return true; }
  S(const E&...) {}
  template<typename... U> S(U&&...) noexcept(n<U...>()) {}
};
template<typename X, typename T> struct F {};
template<typename X, typename... T> using A = S<F<X, T>...>;
A a{F<int, int>{}};
