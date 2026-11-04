//type: fp
//options: --c++26 --gn 160100:--c++26 --gn 160100 -tused
//remark: [GH #216] GNU integer packs in expansion initializers.

namespace concrete_integer_pack
{
  constexpr int f()
  {
    int n = 0;
    template for (auto x : { __integer_pack(3)... })
    {
      n = n * 10 + x + 1;
    }
    return n;
  }
  static_assert(f() == 123);
}

namespace empty_integer_pack
{
  constexpr int f()
  {
    int n = 0;
    template for (auto x : { __integer_pack(0)... }) { ++n; }
    template for (auto x : { __integer_pack(0)..., 4,
                            __integer_pack(0)..., 5,
                            __integer_pack(0)..., })
    {
      n = n * 10 + x;
    }
    return n;
  }
  static_assert(f() == 45);
}

namespace dependent_integer_pack
{
  template<int N> constexpr int f()
  {
    int n = 0;
    template for (auto x : { __integer_pack(N)... })
    {
      n = n * 10 + x + 1;
    }
    return n;
  }
  static_assert(f<0>() == 0);
  static_assert(f<1>() == 1);
  static_assert(f<3>() == 123);
}

namespace nested_integer_pack
{
  constexpr int f()
  {
    int n = 0;
    template for (constexpr auto x : { 0, 1, 3 })
    {
      template for (auto y : { __integer_pack(x)... })
      {
        n += y + 1;
      }
    }
    return n;
  }
  static_assert(f() == 7);
}

namespace shared_list_scanners
{
  template<typename... T> constexpr int g(T... x)
  {
    return (0 + ... + x);
  }

  template<int N> constexpr int f()
  {
    int a[] = { 7, __integer_pack(N)..., 9 };
    return g(__integer_pack(N)...) + a[0] + a[N + 1];
  }
  static_assert(f<0>() == 16);
  static_assert(f<1>() == 16);
  static_assert(f<3>() == 19);

  struct S
  {
    int n;
    template<typename... T> constexpr S(T... x) : n(g(x...)) {}
  };

  template<int N> struct C
  {
    S s;
    constexpr C() : s(__integer_pack(N)...) {}
  };
  static_assert(C<0>{}.s.n == 0);
  static_assert(C<1>{}.s.n == 0);
  static_assert(C<3>{}.s.n == 3);
}
