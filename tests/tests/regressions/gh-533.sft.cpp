//type:fp
//options:--c++20:--c++20 --g++ --gnu_version=150200:--c++20 --clang --clang_version=220100:--ms_c++20 --microsoft_version 1950

// Class template argument deduction through an alias template read a
// destroyed array (the mapping of template parameter packs) when substituting
// into the requires clause of a constructor of a class template with a
// parameter pack, e.g. aliases of std::tuple with libstdc++.

template<typename... E> struct S
{
  static constexpr bool c = true;
  S(E...) requires c {}
};
template<typename... T> using A = S<T...>;
A a{1};
