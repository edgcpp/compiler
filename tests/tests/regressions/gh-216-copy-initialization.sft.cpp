//type: fp
//options: --c++26:--c++26 -tused
//remark: [GH #216] Ordinary copy-initialization of expansion elements.

namespace overload_sets
{
  constexpr int f(int) { return 1; }
  constexpr int f(double) { return 2; }
  template<typename T> constexpr int g(T) { return 3; }

  constexpr int test()
  {
    int n = 0;
    template for (int (*p)(int) : {f, &f, (f), g, g<int>}) {
      n += p(0);
    }
    template for (int (&r)(double) : {f, (f)}) { n += r(0); }
    template for (int (*p)(int) : {{f}, {&f}}) { n += p(0); }
    return n;
  }
  static_assert(test() == 15);

  template<typename T> constexpr int test()
  {
    int n = 0;
    template for (int (*p)(T) : {f, &f, (f), g, g<T>}) {
      n += p(T{});
    }
    return n;
  }
  static_assert(test<int>() == 9);
  static_assert(test<double>() == 12);
}

namespace member_overload_sets
{
  struct S {
    constexpr int f(int) const { return 1; }
    constexpr int f(double) const { return 2; }
  };
  constexpr int test()
  {
    int n = 0;
    template for (int (S::*p)(int) const : {&S::f}) {
      n += (S{}.*p)(0);
    }
    return n;
  }
  static_assert(test() == 1);
}

namespace null_pointer_constants
{
  struct S { int m; };
  struct D {
    int n;
    constexpr D(int*) : n(3) {}
    constexpr D(double) : n(4) {}
  };
  struct C {
    int n;
    constexpr C(int*) : n(1) {}
    constexpr C(...) : n(2) {}
  };
  constexpr int test()
  {
    int n = 0;
    template for (int *p : {0, (0), 0L, nullptr}) { n += p == nullptr; }
    template for (int S::*p : {0, (0), nullptr}) { n += p == nullptr; }
    template for (int (*p)() : {0, nullptr}) { n += p == nullptr; }
    template for (D d : {nullptr, 1.0}) { n += d.n; }
    template for (C c : {0, (0)}) { n += c.n; }
    return n;
  }
  static_assert(test() == 18);

  template<typename T> constexpr bool test()
  {
    template for (T p : {0, (0), nullptr}) {
      if (p != nullptr) return false;
    }
    return true;
  }
  static_assert(test<int*>());
  static_assert(test<int S::*>());
}

namespace array_initializers
{
  constexpr int test()
  {
    int n = 0;
    template for (int a[2] : {{1, 2}, {3}, {}}) {
      n += a[0] + a[1];
      a[0] = 9;
    }
    template for (int a[] : {{1}, {2, 3}}) {
      n += sizeof(a) / sizeof(int) + a[0];
    }
    template for (int a[2][2] : {{{1, 2}, {3, 4}}}) {
      n += a[1][1];
    }
    template for (constexpr int a[] : {{1}, {2, 3}}) {
      static_assert(a[0] > 0);
      n += a[0];
    }
    return n;
  }
  static_assert(test() == 19);

  template<typename T> constexpr int test()
  {
    int n = 0;
    template for (T a[] : {{1}, {2, 3}}) {
      n += sizeof(a) / sizeof(T) + a[0];
    }
    return n;
  }
  static_assert(test<int>() == 6);

  constexpr int dynamic(int i)
  {
    int n = 0;
    template for (int a[2] : {{i++, i++}, {i++}}) {
      n += a[0] + a[1];
    }
    return n * 10 + i;
  }
  static_assert(dynamic(1) == 64);
}

namespace string_initializers
{
  bool test()
  {
    template for (char a[2] : {"a", "b"}) {
      if (a[1] != 0) return false;
      a[0] = 'c';
    }
    template for (char a[] : {"a", "bc"}) {
      if (a[sizeof(a) - 1] != 0) return false;
    }
    template for (const wchar_t a[] : {L"a", {L"bc"}}) {
      if (a[sizeof(a) / sizeof(wchar_t) - 1] != 0) return false;
    }
    return true;
  }
  template<typename T> int test()
  {
    int n = 0;
    template for (T a[] : {"a", "bc"}) {
      static_assert(sizeof(a) >= 2);
      n += sizeof(a);
    }
    return n;
  }
  template int test<char>();
}

namespace array_element_lifetimes
{
  struct S {
    int *p;
    constexpr S(int *q) : p(q) { ++*p; }
    constexpr ~S() { --*p; }
  };
  constexpr bool test()
  {
    int n = 0;
    template for (S a[2] : {{&n, &n}, {&n, &n}}) {
      if (n != 2) return false;
    }
    return n == 0;
  }
  static_assert(test());
}

namespace designated_initializers
{
  struct S { int a; int b; int c; };

  constexpr int test()
  {
    int n = 0;
    template for (S s : {{.b = 2}, {.a = 1, .c = 3}}) {
      n += 100 * s.a + 10 * s.b + s.c;
    }
    return n;
  }
  static_assert(test() == 123);
}

namespace reference_initializers
{
  struct S { int a; int b; };

  constexpr int test()
  {
    int n = 0;
    template for (const S &s : {{1, 2}, {3, 4}}) { n += s.a + s.b; }
    template for (const int (&a)[2] : {{5, 6}}) { n += a[0] + a[1]; }
    template for (const int &r : {7, 8}) { n += r; }
    return n;
  }
  static_assert(test() == 36);

  int test_lifetime()
  {
    int n = 0;
    template for (int (&&a)[2] : {{1, 2}, {3, 4}}) { n += a[0] + a[1]; }
    return n;
  }
  int result = test_lifetime();
}
