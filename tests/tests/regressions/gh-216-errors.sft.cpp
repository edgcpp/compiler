//type: fn
//options: --c++26 --no_warnings:--c++26 --no_warnings --microsoft_version 1950 --ms_permissive
//remark:[GH #216] Definition checking, element diagnostics, and recovery.

namespace definition_checking
{
  void f()
  {
    template for (auto x : {})
    {
      (void)unknown_name;
    }
  }
}

namespace element_checking
{
  void f()
  {
    template for (constexpr auto x : { 1, 2 })
    {
      static_assert(x == 1);
    }
  }
}

namespace constant_binding
{
  void f(int n)
  {
    template for (constexpr int x : { n })
    {
      (void)x;
    }
  }
}

namespace conflicting_declarations
{
  void f()
  {
    template for (int x = 0; auto x : {}) {}
    template for (auto x : { 1 })
    {
      int x;
    }
  }
}

namespace control_flow_boundary
{
  void f(int n)
  {
    switch (n)
    {
      template for (auto x : { 1 })
      {
        case 0: break;
      }
    }
    template for (auto x : { 1 })
    {
      label: (void)x;
    }
  }
}

namespace malformed_header
{
  void f()
  {
    template for (int x) {}
    template for (auto x : { , }) {}
  }
}

namespace restored_context
{
  void f()
  {
    template for (auto x : {}) { static_assert(false); }
    static_assert(false);
  }
}

namespace extern_storage_class
{
  void f()
  {
    template for (extern int x : { 1 })
    { (void)x; }
  }
}

namespace nonconstant_iterator_lambda_capture
{
  void f()
  {
    template for (auto i : { 1 })
    {
      [] () { return i; };
    }
  }
}

namespace goto_into_expansion
{
  void f()
  {
    goto in;
    template for (auto x : { 1 })
    {
    in:
      (void)x;
    }
  }
}

namespace undeclared_local_base
{
  void f()
  {
    template for (auto x : {0}) { struct S : Missing {}; }
  }
}

namespace lvalue_bit_field
{
  struct B { int value : 3; };

  void f()
  {
    B b{};
    template for (auto&& x : b) { (void)x; }
  }
}

namespace lvalue_rvalue_reference_member
{
  struct R { int&& value; };

  void f()
  {
    int n = 0;
    R r{static_cast<int&&>(n)};
    template for (decltype(auto) x : r) { (void)x; }
  }
}

namespace constant_binding_break
{
  void f(int n)
  {
    template for (constexpr int x : { n, 2 })
    {
      break;
    }
  }
}

namespace destructuring_constexpr_error
{
  struct S { int value; };

  void f(int n)
  {
    template for (constexpr auto x : S{n}) { (void)x; }
  }
}

namespace tuple_like_union
{
  union U { int value; };
}

namespace std
{
  template<typename T> struct tuple_size;
  template<unsigned long I, typename T> struct tuple_element;

  template<>
  struct tuple_size<tuple_like_union::U> {
    static constexpr unsigned long value = 1;
  };
  template<>
  struct tuple_element<0, tuple_like_union::U> { using type = int; };
}

namespace tuple_like_union
{
  void f()
  {
    U u{};
    auto&& [x] = u;
    (void)x;
  }
}

namespace unknown_bound_array
{
  extern int a[];

  void f()
  {
    auto&& [x] = a;
    (void)x;
  }
}

namespace unresolved_overload_initializer
{
  void g(int);
  void g(char);

  void f()
  {
    template for (auto x : g) { (void)x; }
    template for (auto x : &g) { (void)x; }
  }
}

namespace incomplete_class
{
  struct S;
  using A = S;
  extern A a;

  void f()
  {
    template for (auto x : a) { (void)x; }
  }

  void g()
  {
    auto&& [x] = a;
    (void)x;
  }
}

namespace invalid_scalar_initializer
{
  void f()
  {
    template for (auto x : (void)0) { (void)x; }
  }
}

namespace invalid_bit_field_initializer
{
  struct S { int value : 3; };

  void f(S s)
  {
    template for (auto x : s.value) { (void)x; }
  }
}

namespace destructuring_constexpr_prvalue
{
  struct S { int value; };

  void f()
  {
    template for (constexpr auto x : S{1}) {}
    template for (constexpr auto&& x : S{1}) {}
  }
}

namespace destructuring_constexpr_empty_prvalue
{
  struct E {};

  void f()
  {
    template for (constexpr auto&& x : E{}) {}
  }
}

namespace destructuring_constexpr_tuple_prvalue
{
  struct T { int value; };

  template<unsigned long I>
  constexpr int get(const T& t) { return t.value; }
}

namespace std
{
  template<typename T> struct tuple_size;
  template<unsigned long I, typename T> struct tuple_element;

  template<>
  struct tuple_size<destructuring_constexpr_tuple_prvalue::T> {
    static constexpr unsigned long value = 1;
  };
  template<>
  struct tuple_element<0, destructuring_constexpr_tuple_prvalue::T> {
    using type = int;
  };
}

namespace destructuring_constexpr_tuple_prvalue
{
  void f()
  {
    template for (constexpr auto x : T{1}) {}
  }
}

namespace destructuring_missing_tuple_get
{
  struct T { int a; int b; };

  template<unsigned long I>
  int& get(T& t) requires (I == 0) { return t.a; }
}

namespace std
{
  template<typename T> struct tuple_size;
  template<unsigned long I, typename T> struct tuple_element;

  template<>
  struct tuple_size<destructuring_missing_tuple_get::T> {
    static constexpr unsigned long value = 2;
  };
  template<unsigned long I>
  struct tuple_element<I, destructuring_missing_tuple_get::T> {
    using type = int;
  };
}

namespace destructuring_missing_tuple_get
{
  void f(T& t)
  {
    int result = 0;
    template for (auto x : t) { result += x; }
    (void)result;
  }
}

namespace private_member_access
{
  class C { int a; int b; public: C(); };

  void f(C c)
  {
    template for (auto x : c) { (void)x; }
  }
}

namespace protected_member_access
{
  struct B { protected: int a; };

  void f(B& b)
  {
    template for (auto x : b) { (void)x; }
  }
}

namespace inaccessible_base_class
{
  class B { public: int a; };
  class D : protected B { };

  void f(D& d)
  {
    template for (auto x : d) { (void)x; }
  }
}
