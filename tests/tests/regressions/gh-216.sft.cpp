//type:fp
//options:--c++26:--c++26 --gn 160100:--c++26 --clang_version 230100:--c++26 --microsoft_version 1952:--c++26 -tused
//remark:[GH #216] C++26 enumerating expansion statements.

namespace simple_enumerating
{
  constexpr int f(int i)
  {
    int sum = 0;

    template for (auto v : { i, 2*i, 3*i })
    {
      sum += v;
    }

    template for (auto v : { i, 2*i, 3*i, })
    {
      sum += v;
    }

    return sum;
  }

  static_assert(f(1) == 12);
  static_assert(f(2) == 24);
}

namespace empty_enumerating
{
  void f()
  {
    template for (auto v : { })
    {
      undefined(v);             // dependent call expression
    }
  }
}

namespace binding_enumerating
{
  struct C
  {
    int i;
  };

  constexpr int f(int i)
  {
    int sum = 0;

    template for (auto [ v ] : { C{ i }, C{ 2*i }, C{ 3*i } })
    {
      sum += v;
    }

    return sum;
  }

  static_assert(f(1) == 6);
  static_assert(f(2) == 12);
}

namespace template_simple_enumerating
{
  constexpr int f(auto i)
  {
    int sum = 0;

    template for (auto v : { i, 2*i, 3*i })
    {
      sum += v;
    }

    return sum;
  }

  static_assert(f(1) == 6);
  static_assert(f(2) == 12);
}

namespace template_binding_enumerating
{
  struct C
  {
    int i;
  };

  constexpr int f(auto i)
  {
    int sum = 0;

    template for (auto [ v ] : { C{ i }, C{ 2*i }, C{ 3*i } })
    {
      sum += v;
    }

    return sum;
  }

  static_assert(f(1) == 6);
  static_assert(f(2) == 12);
}

namespace binding_pack_enumerating
{
  struct C
  {
    int i;
  };

  constexpr int f(int i)
  {
    int sum = 0;

    template for (auto [ ... v ] : { C{ i }, C{ 2*i }, C{ 3*i } })
    {
      sum += (v + ...);
    }

    return sum;
  }

  static_assert(f(1) == 6);
  static_assert(f(2) == 12);
}

namespace template_binding_pack_enumerating
{
  struct C
  {
    int i;
  };

  constexpr int f(auto i)
  {
    int sum = 0;

    template for (auto [ ... v ] : { C{ i }, C{ 2*i }, C{ 3*i } })
    {
      sum += (v + ...);
    }

    return sum;
  }

  static_assert(f(1) == 6);
  static_assert(f(2) == 12);
}

namespace simple_expanding_enumerating
{
  constexpr int f(auto ... i)
  {
    int sum = 0;

    template for (auto v : { i ... })
    {
      sum += v;
    }

    return sum;
  }

  static_assert(f(1, 2, 3) == 6);
  static_assert(f(2, 4, 6) == 12);
}

namespace enumerating_with_init_statement
{
  constexpr int f(int i)
  {
    int sum;

    template for (sum = 0; auto v : { i, 2*i, 3*i })
    {
      sum += v;
    }

    template for (int sum = 0; auto v : { i, 2*i, 3*i })
    {
      sum += v;                 // does not affect outer scope sum
    }

    return sum;
  }

  static_assert(f(1) == 6);
  static_assert(f(2) == 12);
}

namespace enumerating_with_break_statement
{
  constexpr int f(int i)
  {
    int sum;

    template for (sum = 0; auto v : { i, 2*i, 3*i })
    {
      if (v == 1) continue;
      if (v == 3) break;
      sum += v;
    }

    return sum;
  }

  static_assert(f(1) == 2);
  static_assert(f(2) == 12);
}

namespace lambda_capture
{
  template<typename T>
  constexpr T f(T v)
  {
    auto l1 = [=] () {
      template for (T i : { })
      {
        return v;
      }

      return 0;
    };
    static_assert(sizeof(l1) == 1);

    auto l2 = [=] () {
      template for (T i : { v })
      {
        return v;
      }
    };
    static_assert(sizeof(l2) == sizeof(v));

    return l1() + 10*l2();
  }

  static_assert(f(1) == 10);
  static_assert(f(2) == 20);
}

namespace iterator_lambda_capture
{
  // Lambdas appearing in the compound-statement that refer to the variable
  // of the for-range-declaration.
  constexpr int f()
  {
    int r = 0;

    template for (constexpr int i : { 1, 2, 3 })
    {
      auto a = [=]() { return i; };   // implicit by-copy capture
      auto b = [&]() { return i; };   // implicit by-reference capture
      auto c = [i]() { return i; };   // explicit capture
      auto d = []() { return i; };    // value use: no capture needed
      r += a() + b() + c() + d();
    }

    return r;
  }

  static_assert(f() == 24);

  // The uncaptured value use alone (previously a spurious error).
  void g()
  {
    template for (constexpr int i : { 1 } )
    {
      [] () { return i; };
    }
  }

  // The same in a dependent body.
  template<int N>
  constexpr int h()
  {
    int r = 0;

    template for (constexpr int i : { 1, 2 })
    {
      r += []() { return i; }() + N;
    }

    return r;
  }

  static_assert(h<10>() == 23);   // (1+10) + (2+10)
}

namespace static_assert_in_empty_expansion
{
  void f()
  {
    template for (auto v: { })
    {
      static_assert(false);
    }
  }
}

namespace fallthrough_in_expansion
{
  constexpr int f(int q)
  {
    int sum = 0;

    template for (auto v : { 1, 2 })
    {
      switch (q)
      {
        case 1:
          [[fallthrough]];
        case 2:
          sum += v;
          break;
      }
    }

    return sum;
  }

  static_assert(f(1) == 3);
  static_assert(f(3) == 0);
}

namespace braced_aggregate_enumerating
{
  struct Pair { int x, y; };

  constexpr int f()
  {
    int sum = 0;

    template for (Pair p : { { 1, 2 }, { 3, 4 } })
    {
      sum += p.x + p.y;
    }

    return sum;
  }

  static_assert(f() == 10);
}

namespace dependent_braced_enumerating
{
  template<typename T> constexpr int f()
  {
    int sum = 0;

    template for (T p : { { 1, 2 }, { 3, 4 } })
    {
      sum += p.x + p.y;
    }

    return sum;
  }

  static_assert(f<braced_aggregate_enumerating::Pair>() == 10);
}

namespace braced_pack_enumerating
{
  struct Pair { int x, y; };

  template<typename... Ts> constexpr int f(Ts... ts)
  {
    int sum = 0;

    template for (Pair p : { Pair{ ts, ts }... })
    {
      sum += p.x + p.y;
    }

    return sum;
  }

  static_assert(f() == 0);
  static_assert(f(1, 2) == 6);

  constexpr int g()
  {
    int sum = 0;

    template for (Pair p : { { 1 }, { 2 } })
    {
      sum += p.x + p.y;  // omitted members are value-initialized
    }

    return sum;
  }

  static_assert(g() == 3);
}

namespace braced_constructor_enumerating
{
  struct Pair
  {
    int sum;
    constexpr Pair(int a, int b) : sum(a + b) { }
  };

  constexpr int f()
  {
    int sum = 0;

    template for (Pair p : { { 1, 2 }, { 3, 4 } })
    {
      sum += p.sum;
    }

    template for (constexpr Pair p : { { 1, 2 }, { 3, 4 } })
    {
      static_assert(p.sum == 3 || p.sum == 7);
      sum += p.sum;
    }

    return sum;
  }

  static_assert(f() == 20);
}

namespace prototype_extern_isolation
{
  template<int> struct Tag { };

  void f()
  {
    // The prototype must not introduce an external entity that conflicts
    // with the declaration from the first actual element.
    template for (constexpr auto x : { 1 })
    {
      extern int a[x];
      extern Tag<x> fun();
    }
  }

  void same_type()
  {
    template for (constexpr auto x : { 1, 1 })
    {
      extern int b[x];
      extern Tag<x> same_fun();
    }
  }
}

namespace enumerating_initializer_lifetime
{
  struct T
  {
    int& x;
    constexpr T(int& v) : x(v) { }
    constexpr ~T() { x = 42; }
  };

  constexpr const T& identity(const T& t) { return t; }

  constexpr int f()
  {
    int x = 5, sum = 0;

    // Even a temporary passed through a call survives the element body.
    template for (const auto& t : { identity(T{x}) })
    {
      sum += t.x;
    }

    return sum + x;  // cleanup occurs after the body, before this return
  }

  static_assert(f() == 47);
}

namespace lambda_local_label_in_expansion
{
  void f()
  {
    template for (auto x : { 1 })
    {
      /* Labels belong to the lambda, not the expansion. */
      [] { label: goto label; }();
    }
  }
}
