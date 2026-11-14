//type:fp
//options:--c++26:--c++26 --gn 160100:--c++26 --clang_version 230100:--c++26 --microsoft_version 1952:--c++26 -tused
//remark:[GH #216] Enumerating expansion statements in dependent contexts.

/* A constexpr element with a deduced type is usable as a template argument. */
namespace dependent_auto_element
{
  template<int>
  constexpr int v = 1;

  void f()
  {
    template for (constexpr auto x : { 1, 2, 3 })
    {
      static_assert(v<x> != 0);
    }
  }
}

/* A constexpr element of class type is converted to a template argument. */
namespace dependent_class_element
{
  template<int>
  constexpr int v = 1;

  struct S
  {
    int m;
    constexpr S(int i) : m(i) { }
    constexpr operator int() const { return m; }
  };

  void f()
  {
    template for (constexpr S x : { 1, 2, 3 })
    {
      static_assert(v<x> != 0);
    }
  }
}

/* An explicitly instantiated function template expands its body. */
namespace dependent_body_explicit_instantiation
{
  void f(int) { }

  template<typename... Ts> void g(Ts... ts)
  {
    template for (auto x : {1, 2})
    {
      f(ts...);                // call-argument expansion in the body
    }
  }

  template void g<int>(int);   // explicit instantiation of the body
}

/* A member function template expands its body with an enclosing pack. */
namespace dependent_body_member_function
{
  template<typename... Ts> struct C
  {
    constexpr int h(Ts... ts)
    {
      int n = 0;
      template for (auto x : {1, 2})
      {
        n += (x + ... + ts);  // fold over the enclosing pack in the body
      }
      return n;
    }
  };

  static_assert(C<int>{ }.h(10) == 23);   // (1+10)+(2+10)
}

/* Constant evaluation instantiates a function template with an expansion. */
namespace dependent_body_file_scope_constexpr
{
  template<typename... Ts> constexpr int g(Ts... ts)
  {
    int n = 0;
    template for (auto x : {1, 2})
    {
      n += (x + ... + ts);
    }
    return n;
  }

  static_assert(g(10) == 23);            // constant-evaluation instantiation
}

/* Folds, call expansions, and sizeof... see the enclosing pack's size. */
namespace dependent_body_cardinality
{
  constexpr int sumv() { return 0; }
  template<typename... Us> constexpr int sumv(int a, Us... us)
  {
    return a + sumv(us...);
  }

  template<typename... Ts> constexpr int g(Ts... ts)
  {
    int n = 0;
    template for (auto x : {1, 2})
    {
      n += sumv(x, ts...) +      // call-argument expansion
           (x + ... + ts) +      // binary fold, pack on the right
           (ts + ... + 0) +      // binary fold, pack on the left
           sizeof...(Ts) + sizeof...(ts) +
           (ts && ...);          // unary fold (empty identity)
    }
    return n;
  }

  static_assert(g() == 8);
  static_assert(g(1) == 18);
  static_assert(g(1, 2) == 34);
}

/* Pack indexing in the body uses the enclosing pack. */
namespace dependent_body_pack_indexing
{
  template<typename... Ts> constexpr int g(Ts... ts)
  {
    int n = 0;
    template for (auto x : {1, 2})
    {
      n += ts...[0] + ts...[sizeof...(ts) - 1];
    }
    return n;
  }

  static_assert(g(10) == 40);
  static_assert(g(10, 30) == 80);
}

/* Lambdas in the body capture enclosing packs by value and reference. */
namespace dependent_body_lambda_captures
{
  template<typename... Ts> constexpr int g(Ts... ts)
  {
    int n = 0;
    template for (auto x : {1})
    {
      auto l1 = [ts...] { return 1; };             // no pack use in the body
      auto l2 = [ts...] { return (ts + ... + 0); };  // fold over the capture
      auto l3 = [&ts...] { return (ts && ...); };    // capture by reference
      n += l1() + l2() + l3();
    }
    return n;
  }

  static_assert(g() == 2);
  static_assert(g(1) == 3);
  static_assert(g(1, 2) == 5);

  // A unary fold over the capture requires a nonempty pack (no identity
  // for "+").
  template<typename... Ts> constexpr int h(Ts... ts)
  {
    int n = 0;
    template for (auto x : {1})
    {
      auto l = [ts...] { return (ts + ...); };
      n += l();
    }
    return n;
  }

  static_assert(h(5) == 5);
  static_assert(h(5, 6) == 11);
}

/* A nested expansion in a dependent body rescans independently. */
namespace dependent_body_nested_expansion
{
  template<typename... Ts> constexpr int g(Ts... ts)
  {
    int n = 0;
    template for (auto x : {1, 2})
    {
      template for (auto y : {10})
      {
        n += (x + ... + ts) + y;
      }
    }
    return n;
  }

  static_assert(g() == 23);
  static_assert(g(1) == 25);
  static_assert(g(1, 2) == 29);
}

/* A structured binding pack in the element declaration combines with an
   enclosing pack in the body. */
namespace dependent_body_binding_pack
{
  struct C
  {
    int i;
    int j;
  };

  template<typename... Ts> constexpr int g(Ts... ts)
  {
    int n = 0;
    template for (auto [ ...v ] : { C{1, 2}, C{3, 4} })
    {
      n += (v + ...) + (ts + ... + 0);
    }
    return n;
  }

  static_assert(g() == 10);
  static_assert(g(1) == 12);
  static_assert(g(1, 2) == 16);
}

/* Packs from enclosing class and member templates are both visible. */
namespace dependent_body_two_level_packs
{
  template<typename... Ts> struct Outer
  {
    template<typename... Us> struct Inner
    {
      template<typename F> static constexpr int f(F fn, Ts... ts, Us... us)
      {
        int n = 0;
        template for (auto x : {1})
        {
          n += fn(ts...);    // pack of the enclosing class template
          n += fn(us...);    // pack of the enclosing member template
        }
        return n;
      }
    };
  };

  constexpr auto sum = [](auto... a) { return (a + ... + 0); };

  static_assert(Outer<int>::Inner<int>::f(sum, 10, 20) == 30);
  static_assert(Outer<int, int>::Inner<int>::f(sum, 1, 2, 3) == 6);
}

/* Instantiation triggered inside an element body sees the element value. */
namespace dependent_body_triggered_from_expansion
{
  template<typename... Ts> constexpr int g(Ts... ts)
  {
    int n = 0;
    template for (auto x : {1, 2})
    {
      n += (x + ... + ts);
    }
    return n;
  }

  constexpr int drive()
  {
    int n = 0;
    template for (auto v : {1, 2})
    {
      n += g(v);           // instantiation triggered inside an expansion body
    }
    return n;
  }

  static_assert(drive() == 12);   // g(1)=5, g(2)=7
}

/* A generic lambda in the body captures and uses an enclosing pack. */
namespace dependent_body_generic_lambda
{
  template<typename... Ts> constexpr int g(Ts... ts)
  {
    int n = 0;
    template for (auto x : {1})
    {
      auto l = [ts...](auto y) { return y + (ts + ... + 0); };
      n += l(x);
    }
    return n;
  }

  static_assert(g() == 1);
  static_assert(g(10) == 11);
  static_assert(g(10, 20) == 31);
}

/* A Microsoft nonreal instantiation defers expansion to the real one. */
namespace dependent_body_nonreal_context
{
  template<typename... Ts> struct C
  {
    constexpr int h(Ts... ts)
    {
      int n = 0;
      template for (auto x : {1, 2})
      {
        n += (x + ... + ts);
      }
      return n;
    }
  };

  // In Microsoft modes the instantiation of C<T> inside drive is a nonreal
  // instantiation; in other modes the expansion is deferred to the
  // instantiation of drive.
  template<typename T> constexpr int drive(T t)
  {
    C<T> c;
    return c.h(t);
  }

  static_assert(drive(10) == 23);
}

/* Pack classification for nested template packs, both outside and inside
   an expansion body. */
namespace dependent_body_pack_classification
{
  // Nested template parameter packs over an enclosing pack (the pack
  // classification machinery shares the pack expansion query used by the
  // expansion statement paths).
  template<typename... Ts> struct Nontype
  {
    template<Ts... ns> struct Inner { };
  };
  template struct Nontype<int>;
  template struct Nontype<int, char>;

  template<typename... Ts> concept C = (sizeof...(Ts) >= 0);

  template<typename... Ts> struct Constrained
  {
    template<C<Ts>... Us> struct Inner { };
  };
  template struct Constrained<int>;
  template struct Constrained<int, char>;

  // The same constructs inside the body of an expansion statement.
  template<typename... Ts> constexpr int g(Ts... ts)
  {
    int n = 0;
    template for (auto x : {1})
    {
      n += sizeof...(Ts) + (ts + ... + 0);
    }
    return n;
  }

  static_assert(g() == 0);
  static_assert(g(1) == 2);
}

/* constexpr elements remain usable in constant expressions in the body. */
namespace constexpr_expansion_element_list
{
  void f()
  {
    template for (constexpr int v: { 1, 2 })
    {
      static_assert(v != 0);
    }

    template for (constexpr auto v: { 1, 2 })
    {
      static_assert(v != 0);
    }
  }

  template<int N>
  constexpr int h()
  {
    int sum = 0;

    template for (constexpr auto v: { N, 2*N })
    {
      constexpr int w = v;
      sum += w;
    }

    return sum;
  }

  static_assert(h<1>() == 3);
}

/* A braced pack expansion can be rescanned during template argument
   deduction. */
namespace braced_init_rescan
{
  template<int> using A = int;

  struct C
  {
    static constexpr int f(int i) { return i; }
    template<int... Is> using type = A<f({Is...})>;
  };

  template<typename T, typename = typename T::template type<1>>
  int g();

  int i = g<C>();  // Previously an internal error.
}
