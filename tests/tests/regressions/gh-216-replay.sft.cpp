//type: fp
//options: --c++26:--c++26 -tused
//remark:[GH #216] Per-element types, discarded branches, and nested replay.

template<typename T, typename U> constexpr bool same = false;
template<typename T> constexpr bool same<T, T> = true;

namespace heterogeneous_elements
{
  struct S { constexpr int value() const { return 4; } };

  constexpr int f()
  {
    int n = 0;
    template for (auto x : { 1, S{} })
    {
      if constexpr (same<decltype(x), int>)
      {
        n += x;
      }
      else
      {
        n += x.value();
      }
    }
    return n;
  }
  static_assert(f() == 5);
}

namespace dependent_return_type
{
  auto f()
  {
    template for (auto x : { 1 }) { return x; }
  }
  static_assert(same<decltype(f()), int>);
}

namespace deduced_return
{
  constexpr auto f()
  {
    template for (auto i : { 1 }) { return i; }
  }
  auto g()
  {
    template for (auto i : { 1 }) { return; }
  }
  static_assert(f() == 1);
}

namespace nested_element_initializers
{
  constexpr int f()
  {
    int n = 0;
    template for (auto x : { 1, 2 })
    {
      template for (auto y : { x, x + 1 })
      {
        n += y;
      }
    }
    return n;
  }
  static_assert(f() == 8);
}

namespace nondependent_lookup
{
  constexpr int g(int) { return 1; }
  constexpr int g(long) { return 2; }

  constexpr int f()
  {
    int n = 0;
    template for (auto x : { 1, 2 })
    {
      n += g(0);  /* The nondependent call is reused for each element. */
      n += g(x);
    }
    return n;
  }
  static_assert(f() == 4);
}

namespace binding_pack_cardinality
{
  struct S {};
  struct C { int a, b; };

  constexpr int f()
  {
    int n = 0;
    template for (auto [...x] : { S{}, C{1, 2} })
    {
      n += (x + ... + 0);
    }
    return n;
  }
  static_assert(f() == 3);
}

namespace local_class_noexcept
{
  constexpr int f()
  {
    int n = 0;
    template for (constexpr auto x : { 1, 2 })
    {
      struct S
      {
        constexpr int g() const noexcept(x == 1) { return x; }
        void h() noexcept(sizeof(S) == sizeof(int));
        int a;
      };
      static_assert(noexcept(S{}.g()) == (x == 1));
      static_assert(noexcept(S{}.h()));
      n += S{}.g();
    }
    return n;
  }
  static_assert(f() == 3);
}

namespace nondependent_exception_specification
{
  void f()
  {
    template for (auto x : { 1 })
    {
      struct S
      {
        static void g() noexcept(true) {}
        static void h() noexcept(false) {}
        static_assert(&g != nullptr);
        static_assert(&h != nullptr);
      };
      static_assert(noexcept(S::g()));
      static_assert(!noexcept(S::h()));
    }
  }
}

namespace early_exception_specification
{
  constexpr int f()
  {
    int n = 0;
    template for (constexpr auto x : { 1, 2 })
    {
      struct S
      {
        static constexpr int g() noexcept(x == 1) { return x; }
        static_assert(&g != nullptr);
      };
      static_assert(noexcept(S::g()) == (x == 1));
      n += S::g();
    }
    return n;
  }
  static_assert(f() == 3);
}

namespace template_exception_specification
{
  template<int N> constexpr int f()
  {
    int n = 0;
    template for (constexpr auto x : { 1, 2 })
    {
      struct S
      {
        static constexpr int g() noexcept(N == 1) { return x; }
        static_assert(&g != nullptr);
      };
      static_assert(noexcept(S::g()) == (N == 1));
      n += S::g();
    }
    return n;
  }
  static_assert(f<1>() == 3);
  static_assert(f<2>() == 3);
}

namespace nested_class_exception_specification
{
  void f()
  {
    template for (auto x : { 1 })
    {
      struct S
      {
        struct C
        {
          static void g(int n) noexcept(sizeof(n) == sizeof(int)) {}
          static_assert(&g != nullptr);
        };
      };
    }
  }
}
