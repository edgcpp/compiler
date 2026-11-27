//type: rp
//options: --c++26:--c++26 -tused
//remark:[GH #216] Destructuring expansion initialization and bounds lookup.

namespace constant_evaluation {
  struct S { int value; };

  constexpr S make() {
    if consteval {
      return {1};
    } else {
      return {2};
    }
  }

  constexpr int f() {
    int result = 0;
    template for (const int x : make()) {
      result += x;
    }
    return result;
  }
  static_assert(f() == 1);
}

namespace constexpr_value {
  struct S { int value; };

  constexpr int f() {
    constexpr S s{3};
    int result = 0;
    template for (constexpr auto x : s) {
      result += x;
    }
    return result;
  }
  static_assert(f() == 3);
}

namespace constexpr_range_initializer {
  struct S { int value; };
  constexpr S s{3};

  constexpr const S& get() {
    if consteval {
      return s;
    } else {
      throw 1;
    }
  }

  bool f() {
    bool visited = false;
    template for (constexpr auto x : get()) {
      static_assert(x == 3);
      visited = true;
    }
    return visited;
  }
}

namespace constexpr_xvalue {
  struct S { int value; };

  constexpr int f() {
    constexpr S s{3};
    int result = 0;
    template for (constexpr auto x : static_cast<const S&&>(s)) {
      result += x;
    }
    return result;
  }
  static_assert(f() == 3);
}

namespace decltype_auto {
  template<typename T, typename U>
  struct Same { static constexpr bool value = false; };
  template<typename T>
  struct Same<T, T> { static constexpr bool value = true; };

  using A = int[1];
  struct S { int value; };
  struct M { mutable int value; };
  struct L { int& value; };
  struct R { int&& value; };

  void f() {
    A a{};
    const A ca{};
    S s{};
    const S cs{};
    const M cm{};
    int n = 0;
    template for (decltype(auto) x : a) {
      static_assert(Same<decltype(x), int>::value);
    }
    template for (decltype(auto) x : ca) {
      static_assert(Same<decltype(x), const int>::value);
    }
    template for (decltype(auto) x : A{1}) {
      static_assert(Same<decltype(x), int&&>::value);
    }
    template for (decltype(auto) x : s) {
      static_assert(Same<decltype(x), int>::value);
    }
    template for (decltype(auto) x : cs) {
      static_assert(Same<decltype(x), const int>::value);
    }
    template for (decltype(auto) x : cm) {
      static_assert(Same<decltype(x), int>::value);
    }
    template for (decltype(auto) x : S{}) {
      static_assert(Same<decltype(x), int&&>::value);
    }
    template for (decltype(auto) x : L{n}) {
      static_assert(Same<decltype(x), int&>::value);
    }
    template for (decltype(auto) x : R{static_cast<int&&>(n)}) {
      static_assert(Same<decltype(x), int&&>::value);
    }
  }
}

namespace bit_fields {
  struct B { int value : 3; };

  constexpr int f() {
    int result = 0;
    template for (auto&& x : B{3}) {
      static_assert(decltype_auto::Same<decltype(x), int&&>::value);
      result += x;
    }
    template for (decltype(auto) x : B{2}) {
      static_assert(decltype_auto::Same<decltype(x), int&&>::value);
      ++x;
      result += x;
    }
    const B b{1};
    template for (decltype(auto) x : static_cast<const B&&>(b)) {
      static_assert(decltype_auto::Same<decltype(x), const int&&>::value);
      result += x;
    }
    return result;
  }
  static_assert(f() == 7);
}

namespace member_access {
  class C {
    int a;
    int b;
  public:
    constexpr C(int n) : a(n), b(n + 1) {}
    constexpr int f() const {
      int n = 0;
      template for (auto x : *this) { n += x; }
      return n;
    }
  };
  static_assert(C(1).f() == 3);

  struct B {
  protected:
    int a;
    constexpr B(int n) : a(n) {}
  };

  struct D : B {
    constexpr D(int n) : B(n) {}
    constexpr int f() const {
      int n = 0;
      template for (auto x : *this) { n += x; }
      return n;
    }
  };
  static_assert(D(4).f() == 4);
}

namespace unassociated_bounds {
  struct S { int value; };
}

namespace std {
  int* begin(unassociated_bounds::S&);
  int* end(unassociated_bounds::S&);
}

namespace unassociated_bounds {
  constexpr int f() {
    int result = 0;
    S s{4};
    template for (auto x : s) {
      result += x;
    }
    return result;
  }
  static_assert(f() == 4);
}

namespace std {
  template<typename T> struct tuple_size;
  template<unsigned long I, typename T> struct tuple_element;
}

namespace tuple_decltype {
  struct T { int value; };
  struct R { int value; };

  template<unsigned long I>
  int& get(T& t) { return t.value; }
  template<unsigned long I>
  int&& get(T&& t) { return static_cast<int&&>(t.value); }
  template<unsigned long I>
  int& get(R& r) { return r.value; }
  template<unsigned long I>
  int& get(R&& r) { return r.value; }
}

namespace std {
  template<>
  struct tuple_size<tuple_decltype::T> {
    static constexpr unsigned long value = 1;
  };
  template<>
  struct tuple_element<0, tuple_decltype::T> { using type = int; };
  template<>
  struct tuple_size<tuple_decltype::R> {
    static constexpr unsigned long value = 1;
  };
  template<>
  struct tuple_element<0, tuple_decltype::R> { using type = int&; };
}

namespace tuple_decltype {
  void f() {
    T t{};
    template for (decltype(auto) x : t) {
      static_assert(decltype_auto::Same<decltype(x), int>::value);
    }
    template for (decltype(auto) x : T{}) {
      static_assert(decltype_auto::Same<decltype(x), int&&>::value);
    }
    template for (decltype(auto) x : R{}) {
      static_assert(decltype_auto::Same<decltype(x), int&>::value);
    }
  }
}

namespace ordered_tuple_bindings {
  struct T {
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
  struct tuple_size<ordered_tuple_bindings::T> {
    static constexpr unsigned long value = 2;
  };
  template<>
  struct tuple_element<0, ordered_tuple_bindings::T> { using type = int; };
  template<>
  struct tuple_element<1, ordered_tuple_bindings::T> { using type = int&; };
}

namespace ordered_tuple_bindings {
  constexpr bool f() {
    T t{0, 0};
    int signature = 0;
    template for (decltype(auto) x : t) {
      signature = 10 * signature +
                  decltype_auto::Same<decltype(x), int&>::value;
      if (t.calls == 12) ++x;
    }
    return signature == 1 && t.value == 1 && t.calls == 12;
  }
  static_assert(f());
}

namespace member_get {
  struct X {
    int value;
    constexpr X(int n) : value(n) {}
    constexpr ~X() {}
  };
  struct T {
    X x;
    template<unsigned long I>
    constexpr X get() const { return x; }
  };
}

namespace std {
  template<>
  struct tuple_size<member_get::T> {
    static constexpr unsigned long value = 1;
  };
  template<>
  struct tuple_element<0, member_get::T> {
    using type = member_get::X;
  };
}

namespace member_get {
  constexpr bool f() {
    constexpr T t{{3}};
    bool result = false;
    template for (constexpr auto x : t) {
      result = x.value == 3;
    }
    return result;
  }
  static_assert(f());
}

namespace unused_tuple_element {
  struct Ignore { constexpr Ignore(const int&) {} };
  struct T { int value; };

  template<unsigned long I>
  constexpr int& get(T& t) { return t.value; }
  template<unsigned long I>
  constexpr int&& get(T&& t) { return static_cast<int&&>(t.value); }
}

namespace std {
  template<>
  struct tuple_size<unused_tuple_element::T> {
    static constexpr unsigned long value = 1;
  };
  template<>
  struct tuple_element<0, unused_tuple_element::T> { using type = int; };
}

namespace unused_tuple_element {
  constexpr bool f() {
    constexpr T t{3};
    template for (constexpr Ignore x : t) {}
    return true;
  }
  static_assert(f());
}

namespace constexpr_tuple_bindings {
  struct T { int a; short b; long c; };

  template<unsigned long I>
  constexpr int get(const T& t) {
    return I == 0 ? t.a : I == 1 ? t.b : t.c;
  }
}

namespace std {
  template<>
  struct tuple_size<constexpr_tuple_bindings::T> {
    static constexpr unsigned long value = 3;
  };
  template<>
  struct tuple_element<0, constexpr_tuple_bindings::T> { using type = int; };
  template<>
  struct tuple_element<1, constexpr_tuple_bindings::T> {
    using type = short;
  };
  template<>
  struct tuple_element<2, constexpr_tuple_bindings::T> { using type = long; };
}

namespace constexpr_tuple_bindings {
  constexpr int f() {
    constexpr T t{1, 2, 3};
    int result = 0;
    template for (constexpr auto x : t) {
      result += x;
    }
    return result;
  }
  static_assert(f() == 6);
}

namespace constexpr_destructor {
  struct D {
    int value;
    constexpr D(int n = 0) : value(n) {}
    constexpr ~D() { value = 0; }
  };
  struct S { D d; };

  constexpr int f() {
    constexpr S s{{3}};
    int result = 0;
    template for (constexpr auto x : s) {
      result += x.value;
    }
    return result;
  }
  static_assert(f() == 3);
}

namespace constexpr_empty_destructor {
  struct E { constexpr ~E() {} };

  constexpr bool f() {
    constexpr E e{};
    template for (constexpr auto&& x : e) {}
    return true;
  }
  static_assert(f());
}

namespace control_flow {
  struct S { int a; int b; int c; };

  constexpr int f() {
    int result = 0, visits = 0;
    template for (auto x : S{1, 2, 3}) {
      ++visits;
      if (x == 2) continue;
      result += x;
      if (visits == 3) break;
    }
    return result * 10 + visits;
  }
  static_assert(f() == 43);
}

namespace lvalue_only_bounds {
  struct S { int value; };

  void begin(S&);
  void end(S&);

  constexpr int f() {
    int result = 0;
    template for (auto x : S{4}) {
      result += x;
    }
    S s{5};
    template for (auto x : static_cast<S&&>(s)) {
      result += x;
    }
    return result;
  }
  static_assert(f() == 9);
}

namespace incomplete_tuple_like {
  struct A;
  int value = 4;

  template<unsigned long I>
  int& get(A&);
}

namespace std {
  template<>
  struct tuple_size<incomplete_tuple_like::A> {
    static constexpr unsigned long value = 1;
  };
  template<>
  struct tuple_element<0, incomplete_tuple_like::A> { using type = int; };
}

namespace incomplete_tuple_like {
  template<unsigned long I>
  int& get(A&) { return value; }

  void f(A& a) {
    int result = 0;
    template for (auto x : a) { result += x; }
    (void)result;
  }
}

namespace incomplete_tuple_like_zero {
  template<typename T> struct B;
}

namespace std {
  template<typename T>
  struct tuple_size<incomplete_tuple_like_zero::B<T>> {
    static constexpr unsigned long value = 0;
  };
}

namespace incomplete_tuple_like_zero {
  void f(B<int>& b) {
    template for (auto x : b) { (void)x; }
  }
}

namespace incomplete_template_instance {
  template<typename T1, typename T2>
  struct pair {
    T1 a;
    T2 b;
  };

  const pair<int, int>& f();

  int g() {
    const auto& [a, b] = f();
    return a + b;
  }

  const pair<int, int>& f() {
    static const pair<int, int> p{1, 2};
    return p;
  }
}

namespace function_reference_components {
  using F = int();
  struct S { F&& value; };
  struct T {};
  constexpr int g() { return 1; }

  template<unsigned long I>
  constexpr F&& get(T&&) { return g; }
}

namespace std {
  template<>
  struct tuple_size<function_reference_components::T> {
    static constexpr unsigned long value = 1;
  };
  template<>
  struct tuple_element<0, function_reference_components::T> {
    using type = function_reference_components::F&&;
  };
}

namespace function_reference_components {
  constexpr int f() {
    int result = 0;
    template for (decltype(auto) x : S{g}) {
      static_assert(decltype_auto::Same<decltype(x), F&>::value);
      result += x();
    }
    template for (decltype(auto) x : T{}) {
      static_assert(decltype_auto::Same<decltype(x), F&>::value);
      result += x();
    }
    return result;
  }
  static_assert(f() == 2);
}

int main() {
  return constant_evaluation::f() != 2 || constexpr_value::f() != 3 ||
         !constexpr_range_initializer::f() || !ordered_tuple_bindings::f();
}
