//type:fp
//options:--c++17:--gnu_version 150200:--clang_version 220100:--ms_c++17 --microsoft_version 1950

// A class template specialization whose nontype template argument refers to
// a dependent static data member (P<b> below) was first created by a member
// declaration and later reused for the same specialization in a constructor
// parameter type.  The first argument expression lacked the information
// needed to redo semantic analysis, so building the implicit deduction guide
// for class template argument deduction caused an internal error ("missing
// default rescan info").

template<typename, typename> constexpr bool f() { return true; }

template<int> struct P {};

template<typename T> struct A {
  static constexpr bool b = f<T, char>();
  P<b> m;
  A(T, P<b>);
};

template<typename T> struct B {
  static constexpr bool b = sizeof(T) == 1;
  P<b> m;
  B(T, P<b>);
};

void g() {
  A{char{}, {}};
  B{char{}, {}};
}
