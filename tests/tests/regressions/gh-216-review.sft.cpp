//type: fp
//options: --c++26:--c++26 -tused
//remark: [GH #216] Expansion initializer emission, packs, and reachability.

namespace embedded_initializer_declarations
{
  constexpr int f()
  {
    int n = 0;
    template for (auto x : { sizeof(struct S*), sizeof(struct C*) })
    {
      n += x != 0;
    }
    return n;
  }
  static_assert(f() == 2);
}

namespace initializer_variable_uses
{
  constexpr int f()
  {
    int a = 1, b = 2, c = 3;
    int n = 0;
    template for (auto x : { (a + b) + c, (b = a, b + c) })
    {
      n += x;
    }
    return n;
  }
  static_assert(f() == 10);
}

namespace discarded_initializer_variable_uses
{
  void f()
  {
    int a = 0, b = 2;
    /* Only b contributes to the element's value. */
    template for (auto x : { (a = 1, b) }) {}
  }
}

namespace nested_binding_pack_restoration
{
  struct S { int a, b; };
  constexpr void g(int, int) {}
  constexpr int h(int a, int b) { return a * 10 + b; }

  constexpr int f()
  {
    int n = 0;
    template for (auto [...x] : { S{1, 2} })
    {
      n = h((g(x...), x)...);
    }
    return n;
  }
  static_assert(f() == 12);

  template<typename T> constexpr int f()
  {
    int n = 0;
    template for (auto [...x] : { T{1, 2} })
    {
      n = h((g(x...), x)...);
    }
    return n;
  }
  static_assert(f<S>() == 12);
}

namespace throwing_init_statement
{
  int f()
  {
    template for (throw 0; int x : { 1 }) {}
  }

  int g()
  {
    template for (throw 0; int x : {}) {}
  }

  template<typename T> int h()
  {
    template for (throw 0; auto x : { T{} }) {}
  }
  template int h<int>();
}
