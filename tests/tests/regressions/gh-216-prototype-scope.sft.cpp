//type: cp
//options: --c++26 --no_warnings -tused

namespace expansion_name_context
{
  template<int> struct S {};

  void f()
  {
    template for (constexpr int n = 1; auto p : {&n})
    {
      S<sizeof(&n)> s;
      S<sizeof(p)> t;
      (void)s;
      (void)t;
    }
  }
}

namespace comma_initializer_element
{
  constexpr int f()
  {
    int n = 0;
    template for (auto x : {(++n, 2)}) { n += x; }
    return n;
  }
  static_assert(f() == 3);
}

namespace init_statement_order
{
  constexpr int f()
  {
    int n = 0;
    template for (n = 1; auto x : {++n}) { n += x; }
    return n;
  }
  static_assert(f() == 4);
}

namespace alias_semantics {
  template<typename T, typename U> constexpr bool same = false;
  template<typename T> constexpr bool same<T, T> = true;

  constexpr int f() {
    int n = 0;
    template for (constexpr int i : {1, 2}) {
      using T = int;
      typedef int T;
      static_assert(same<T, int>);
      static_assert(same<const T&, const int&>);
      T x = i;
      template for (constexpr int j : {3}) {
        using U = T;
        static_assert(same<U, int>);
        n += x + j;
      }
    }
    return n;
  }
  static_assert(f() == 9);
}

namespace local_declarations {
  int g();
  void h(int);

  inline void f() {
    template for (int n = g(); constexpr int i : {1, 2}) {
      using T = int;
      struct S {
        T x;
        int f() const { return x; }
      };
      static S s = { g() };
      {
        typedef int U;
        static U u = g();
        h(s.f() + u + i + n);
      }
    }
    template for (constexpr int i : {}) {
      typedef int T;
      struct S { T x; };
      static S s = {g()};
      h(s.x + i);
    }
  }
  void use() { f(); }
}

namespace template_context {
  template<typename T> inline void f() {
    template for (constexpr int i : {1, 2}) {
      using U = int;
      typedef int U;
      static_assert(alias_semantics::same<U, int>);
      struct S { U x; };
      static S s = {i};
      local_declarations::h(s.x);
    }
  }
  void use() { f<int>(); }
}

namespace local_enum_and_class
{
  /* Local enumerations and classes in the prototype body are nonreal
     entities, but the per-element rescans create real ones. */
  int f()
  {
    int r = 0;
    template for (auto x : { 1, 2 })
    {
      enum E { a = 1, b = 2 };
      struct L { int m() const { return 3; } };
      L l;
      r += x + a + b + l.m();
    }
    return r;
  }

  template<typename T> int g()
  {
    int r = 0;
    template for (auto x : { 1, 2 })
    {
      enum E { a = 1, b = 2 };
      struct L { int m() const { return sizeof(T); } };
      L l;
      r += x + a + b + l.m();
    }
    return r;
  }

  int use() { return f() + g<int>(); }
}
