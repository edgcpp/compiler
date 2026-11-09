//type:fp
//options:--c++17 --g++:--c++17 --clang:--ms_c++17 --microsoft --microsoft_version 1936:--ms_c++17 --microsoft --microsoft_version 1936 --dep_name:--c++17
//remark:Default template arguments of a member class template substituted
//remark:inside an expression rescan of a partial specialization.

// The default argument of a member class template was substituted in the
// context of that template while an expression of a partial specialization
// was being rescanned.  Pushing that context clears the expression stack, but
// the substitution was still done as if inside the expression rescan, which
// caused an internal error.

template<typename...> using void_t = void;

// The original case: decltype of a static member of a member template.
template<typename T, typename = void> struct D {
  static constexpr bool value = false;
};
template<typename T>
struct D<T, void_t<decltype(T::template A<int>::value)>> {
  static constexpr bool value = true;
};
struct S {
  template<typename T, typename = decltype(T() + 1)>
  struct A { static constexpr int value = 0; };
};
static_assert(D<S>::value, "");

// Default argument failing substitution: the specialization isn't used.
struct Bad {
  template<typename T, typename = decltype(T() + 1)>
  struct A { static constexpr int value = 0; };
};
struct NoPlus {};
template<typename T, typename = void> struct D2 {
  static constexpr bool value = false;
};
template<typename T>
struct D2<T, void_t<decltype(T::template A<NoPlus>::value)>> {
  static constexpr bool value = true;
};
static_assert(!D2<Bad>::value, "");

// Expression in sizeof, with chained defaults.
struct C {
  template<typename T, typename U = decltype(T() * 2),
           typename V = decltype(U() - 1)>
  struct A { V v; };
};
template<typename T, typename = void> struct D3 {
  static constexpr bool value = false;
};
template<typename T>
struct D3<T, void_t<decltype(sizeof(typename T::template A<long>))>> {
  static constexpr bool value = true;
};
static_assert(D3<C>::value, "");

// Variable template partial specialization, with a pack.
template<typename, typename T, typename... P>
constexpr bool v = false;
template<typename T, typename... P>
constexpr bool v<void_t<decltype(T::template A<P...>::value)>, T, P...> =
    true;
static_assert(v<void, S, int>, "");
static_assert(!v<void, Bad, NoPlus>, "");

// Function template overload resolution (SFINAE in a return type).
template<typename T>
auto f(int) -> decltype(T::template A<int>::value, true) { return true; }
template<typename T>
bool f(...) { return false; }
bool b = f<S>(0);
