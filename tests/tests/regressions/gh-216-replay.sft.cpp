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
