//type: rp
//options: --c++26:--c++26 -tused
//remark:[GH #216] Destructuring expansion statements in dependent contexts.

/* The initializer type is known only at instantiation. */
namespace dependent_initializer_type
{
  template<typename T, typename U>
  struct Same { static constexpr bool value = false; };
  template<typename T>
  struct Same<T, T> { static constexpr bool value = true; };

  struct S { int a; int b; };

  template<typename T>
  constexpr int f() {
    T t{1, 2};
    int n = 0;
    template for (auto x : t) {
      static_assert(Same<decltype(x), int>::value);
      n += x;
    }
    return n;
  }
  static_assert(f<S>() == 3);
}

/* The array bound and the element type are known only at instantiation. */
namespace dependent_array_initializer
{
  template<typename T, typename U>
  struct Same { static constexpr bool value = false; };
  template<typename T>
  struct Same<T, T> { static constexpr bool value = true; };

  template<typename T, int N>
  constexpr int f() {
    T a[N] = {1, 2, 3};
    int n = 0;
    template for (decltype(auto) x : a) {
      static_assert(Same<decltype(x), T>::value);
      n += x;
    }
    return n;
  }
  static_assert(f<int, 3>() == 6);
}

/* A tuple-like initializer whose get functions and tuple_element depend
   on T. */
namespace dependent_tuple_initializer
{
  template<typename T>
  struct Holder { T value; };

  template<unsigned long I, typename T>
  constexpr T& get(Holder<T>& h) { return h.value; }
}

namespace std {
  template<typename T> struct tuple_size;
  template<unsigned long I, typename T> struct tuple_element;

  template<typename T>
  struct tuple_size<dependent_tuple_initializer::Holder<T>> {
    static constexpr unsigned long value = 2;
  };
  template<typename T, unsigned long I>
  struct tuple_element<I, dependent_tuple_initializer::Holder<T>> {
    using type = T;
  };
}

namespace dependent_tuple_initializer
{
  template<typename T>
  constexpr int f(T init) {
    Holder<T> h{init};
    int n = 0;
    template for (decltype(auto) x : h) {
      n += x;
    }
    return n;
  }
  static_assert(f(1) == 2);
}

/* The get calls are made in index order even when they have side
   effects. */
namespace dependent_tuple_binding_order
{
  struct Ordered {
    int value;
    int calls;

    template<unsigned long I>
    constexpr int& get() & {
      calls = 10 * calls + (I + 1);
      return value;
    }
  };
}

namespace std {
  template<>
  struct tuple_size<dependent_tuple_binding_order::Ordered> {
    static constexpr unsigned long value = 2;
  };
  template<unsigned long I>
  struct tuple_element<I, dependent_tuple_binding_order::Ordered> {
    using type = int;
  };
}

namespace dependent_tuple_binding_order
{
  template<typename T, typename U>
  struct Same { static constexpr bool value = false; };
  template<typename T>
  struct Same<T, T> { static constexpr bool value = true; };

  template<typename T>
  constexpr bool f(T) {
    Ordered o{1, 0};
    int signature = 0;
    template for (decltype(auto) x : o) {
      static_assert(Same<decltype(x), int>::value);
      signature = 10 * signature + (x > 0);
    }
    return signature == 11 && o.calls == 12;
  }
  static_assert(f(0));
}

/* A constexpr for-range-declaration is checked at instantiation. */
namespace dependent_constexpr_element
{
  template<int>
  constexpr int v = 1;

  struct S { int a; int b; };

  template<typename T>
  constexpr int f() {
    constexpr T t{1, 2};
    int n = 0;
    template for (constexpr auto x : t) {
      static_assert(v<x> != 0);
      n += x;
    }
    return n;
  }
  static_assert(f<S>() == 3);
}

/* The element binds to a component of a dependent member type. */
namespace dependent_member_type
{
  template<typename T, typename U>
  struct Same { static constexpr bool value = false; };
  template<typename T>
  struct Same<T, T> { static constexpr bool value = true; };

  template<typename T>
  struct Pair { T first; T second; };

  template<typename T>
  constexpr int f() {
    Pair<T> p{1, 2};
    int n = 0;
    template for (const auto& x : p) {
      static_assert(Same<decltype(x), const T&>::value);
      n += x;
    }
    return n;
  }
  static_assert(f<int>() == 3);
}

/* The body combines the element with an enclosing pack. */
namespace dependent_body_enclosing_pack
{
  struct S { int a; int b; };

  template<typename... Ts>
  constexpr int g(Ts... ts) {
    S t{1, 2};
    int n = 0;
    template for (auto x : t) {
      n += x + sizeof...(Ts) + (ts + ... + 0);
    }
    return n;
  }
  static_assert(g() == 3);
  static_assert(g(10) == 25);
  static_assert(g(10, 20) == 67);
}

/* A nested destructuring expansion inside a dependent body. */
namespace dependent_body_nested_destructuring
{
  struct S { int a; int b; };

  template<typename T>
  constexpr int f() {
    T t{1, 2};
    int n = 0;
    template for (auto x : t) {
      template for (auto y : t) {
        n += x * y;
      }
    }
    return n;
  }
  static_assert(f<S>() == 9);
}

/* A structured binding pack declared in the init-statement of a destructuring
   expansion is not confused with the synthesized range variable. */
namespace dependent_init_statement_binding_pack
{
  struct P { int a; int b; };

  template<typename T>
  constexpr int f(T init) {
    P p{1, 2};
    auto [x, ...rest] = p;
    dependent_tuple_initializer::Holder<T> h{init};
    int n = 0;
    template for (auto e : h) {
      n += x + (rest + ...) + e;
    }
    return n;
  }
  static_assert(f(3) == 12);
}

/* A pack from an outer expansion's init-statement referenced from a nested
   destructuring expansion. */
namespace dependent_nested_init_statement_binding_pack
{
  struct P { int a; int b; };

  template<typename T>
  constexpr int f(T init) {
    P p{1, 2};
    auto [x, ...rest] = p;
    dependent_tuple_initializer::Holder<T> h{init};
    int n = 0;
    template for (auto e : h) {
      template for (auto e2 : h) {
        n += x + (rest + ...) + e + e2;
      }
    }
    return n;
  }
  static_assert(f(3) == 36);
}

/* Instantiation triggered from inside an element body sees the element. */
namespace dependent_body_triggered_from_expansion
{
  struct S { int a; int b; };

  template<typename T>
  constexpr int g(T t) { return t * 10; }

  template<typename T>
  constexpr int f() {
    T t{1, 2};
    int n = 0;
    template for (auto x : t) {
      n += g(x);
    }
    return n;
  }
  static_assert(f<S>() == 30);
}

/* The expansion is deferred in a prototype and replayed in a real
   instantiation. */
namespace dependent_replay_explicit_instantiation
{
  struct S { int a; int b; };

  template<typename T>
  int f() {
    T t{1, 2};
    int n = 0;
    template for (auto x : t) {
      n += x;
    }
    return n;
  }

  template int f<S>();

  int drive() { return f<S>(); }
}

int main() {
  return dependent_replay_explicit_instantiation::drive() != 3;
}
