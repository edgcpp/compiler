//type:fp
//options_all:--c++26
//remark:P3068R6: Allowing exception throwing in constant evaluation

namespace std {
#pragma define_type_info
  class type_info {
  public:
    virtual ~type_info();
  };
  class exception {
  public:
    constexpr exception() noexcept {}
    constexpr virtual ~exception() noexcept {}
    constexpr virtual const char* what() const noexcept { return "exc"; }
  };
}

constexpr bool eq(const char* a, const char* b) {
  while (*a == *b && *a) { ++a; ++b; }
  return *a == *b;
}

struct Counted {
  int* p;
  constexpr Counted(int* p) : p(p) {}
  constexpr Counted(const Counted& o) : p(o.p) {}
  constexpr ~Counted() { ++*p; }
};

struct Thrower {
  constexpr Thrower(int*, bool t = true) { if (t) throw 5; }
};

namespace handlers {
  struct B {
    int v;
    constexpr B(int v) : v(v) {}
    constexpr virtual int get() const { return v; }
  };
  struct D : B {
    constexpr D(int v) : B(v) {}
    constexpr int get() const override { return v * 10; }
  };
  struct M { int pad; };
  struct DM : M, B { constexpr DM(int v) : M{0}, B(v) {} };

  constexpr int scalar() {
    try { throw 42; } catch (int i) { return i; }
    return -1;
  }
  static_assert(scalar() == 42);

  constexpr int mismatch() {
    try {
      try { throw 1.5; } catch (int) { return -1; }
    } catch (double d) { return d * 2; }
    return -2;
  }
  static_assert(mismatch() == 3);

  constexpr int by_base_ref() {
    try { throw D(7); } catch (B& b) { return b.v; }
    return -1;
  }
  static_assert(by_base_ref() == 7);

  constexpr int by_base_val() {
    try { throw D(8); } catch (B b) { return b.get(); }
    return -1;
  }
  static_assert(by_base_val() == 8);

  constexpr int virt_via_ref() {
    try { throw D(4); } catch (const B& b) { return b.get(); }
    return -1;
  }
  static_assert(virt_via_ref() == 40);

  constexpr int nonzero_offset() {
    try { throw DM(5); } catch (B& b) { return b.v; }
    return -1;
  }
  static_assert(nonzero_offset() == 5);

  constexpr int ellipsis() {
    try { throw D(9); } catch (int) { return -1; } catch (...) { return 9; }
    return -2;
  }
  static_assert(ellipsis() == 9);

  constexpr D gd(3);
  constexpr int ptr_to_base() {
    try { throw &gd; } catch (const B* p) { return p->get(); }
    return -1;
  }
  static_assert(ptr_to_base() == 30);

  constexpr int nullp() {
    try { throw nullptr; } catch (int* p) { return p == nullptr ? 3 : -1; }
    return -2;
  }
  static_assert(nullp() == 3);

  constexpr int ref_to_ptr() {
    int i = 4;
    try { throw &i; } catch (const int* const& p) { return *p; }
    return -1;
  }
  static_assert(ref_to_ptr() == 4);

  constexpr int ref_to_base_ptr() {
    try { throw &gd; } catch (const B* const& p) { return p->get(); }
    return -1;
  }
  static_assert(ref_to_base_ptr() == 30);

  constexpr int ref_to_null_ptr() {
    try { throw nullptr; } catch (const int* const& p) { return p ? -1 : 2; }
    return -2;
  }
  static_assert(ref_to_null_ptr() == 2);

  struct S {
    int m;
    constexpr int f() const noexcept { return m + 1; }
  };
  constexpr int ptr_to_member() {
    S s{3};
    try { throw &S::m; } catch (const int S::* p) { return s.*p; }
    return -1;
  }
  static_assert(ptr_to_member() == 3);

  constexpr int same_ptr_to_member() {
    S s{4};
    try { throw &S::m; } catch (int S::* p) { return s.*p; }
    return -1;
  }
  static_assert(same_ptr_to_member() == 4);

  struct VD : virtual B {};
  constexpr int null_ptr_to_virtual_base() {
    try { throw (VD*)nullptr; } catch (B* p) { return p ? -1 : 1; }
    return -2;
  }
  static_assert(null_ptr_to_virtual_base() == 1);

  constexpr int null_ptr_to_member() {
    try { throw nullptr; } catch (int S::* p) { return p ? -1 : 1; }
    return -2;
  }
  static_assert(null_ptr_to_member() == 1);

  constexpr int null_ptr_to_member_function() {
    try {
      throw nullptr;
    } catch (int (S::* const& p)() const) {
      return p ? -1 : 1;
    }
    return -2;
  }
  static_assert(null_ptr_to_member_function() == 1);

  constexpr int noexcept_ptr_to_member_function() {
    S s{6};
    try { throw &S::f; } catch (int (S::*p)() const) { return (s.*p)(); }
    return -1;
  }
  static_assert(noexcept_ptr_to_member_function() == 7);

  constexpr int seven() noexcept { return 7; }
  constexpr int noexcept_function_ptr() {
    try { throw &seven; } catch (int (*p)()) { return p(); }
    return -1;
  }
  static_assert(noexcept_function_ptr() == 7);

  constexpr int noexcept_function_ptr_ref() {
    try { throw &seven; } catch (int (* const& p)()) { return p() + 1; }
    return -1;
  }
  static_assert(noexcept_function_ptr_ref() == 8);

  struct PM { int i; int* p; };
  constexpr int ptr_to_member_quals() {
    int r = 0;
    try { throw &PM::i; } catch (const int PM::*) { r += 1; }
    try {
      throw &PM::p;
    } catch (const int* PM::*) {   // Not a qualification conversion.
      r += 100;
    } catch (const int* const PM::*) {
      r += 10;
    }
    return r;
  }
  static_assert(ptr_to_member_quals() == 11);

  // A pointer to an object that is not usable in constant expressions,
  // converted to a pointer to a base class at a nonzero offset.
  struct RA { int a = 1; };
  struct RB { int b = 2; };
  struct RC : RB { int c = 3; };
  struct RD : RA, RC { int d = 4; };
  RD rd;
  constexpr int runtime_ptr_to_base() {
    int r = 0;
    try { throw &rd; } catch (RB* p) { r += p == &rd; }
    try { throw &rd; } catch (RC* const& p) { r += 10 * (p == &rd); }
    return r;
  }
  static_assert(runtime_ptr_to_base() == 11);

  constexpr int dynamic_exc() {
    int* p = new int(6);
    try { throw p; } catch (int* q) { int r = *q; delete q; return r; }
    return -1;
  }
  static_assert(dynamic_exc() == 6);

  constexpr int ternary(bool b) { return b ? 1 : throw 2; }
  constexpr int use_ternary() {
    try { return ternary(false); } catch (int i) { return i; }
  }
  static_assert(use_ternary() == 2);
  static_assert(ternary(true) == 1);
}

namespace rethrow {
  constexpr int simple() {
    int n = 0;
    try {
      try { throw 4; } catch (int i) { n = i; throw; }
    } catch (int j) { n += j; }
    return n;
  }
  static_assert(simple() == 8);

  constexpr int modify_and_rethrow() {
    try {
      try { throw 1; } catch (int& i) { i = 7; throw; }
    } catch (int j) { return j; }
    return -1;
  }
  static_assert(modify_and_rethrow() == 7);

  constexpr int nested_handlers() {
    int r = 0;
    try {
      throw 1;
    } catch (int) {
      try { throw 2.0; } catch (double) { r = 2; }
      try { throw; } catch (int i) { r += i * 10; }
    }
    return r;
  }
  static_assert(nested_handlers() == 12);

  constexpr int throw_in_handler() {
    int n = 0;
    try {
      try { throw Counted(&n); } catch (...) { throw 5; }
    } catch (int i) {
      n += 10 * i;
    }
    return n;
  }
  static_assert(throw_in_handler() == 51);

  // A destructor that runs during the unwinding for a rethrow rethrows the
  // same exception (which is still being handled) and catches it.
  struct RethrowInDtor {
    constexpr ~RethrowInDtor() { try { throw; } catch (int& i) { i += 10; } }
  };
  constexpr int rethrow_during_rethrow() {
    try {
      try { throw 1; } catch (int) { RethrowInDtor r; throw; }
    } catch (int i) {
      return i;
    }
    return -1;
  }
  static_assert(rethrow_during_rethrow() == 11);
}

namespace unwinding {
  constexpr void thrower(int i) { if (i > 2) throw i; }
  constexpr int across_calls() {
    int n = 0;
    for (int i = 0; i < 10; ++i) {
      try { thrower(i); n += 1; } catch (int k) { n += 100 * k; break; }
    }
    return n;
  }
  static_assert(across_calls() == 303);

  constexpr int deep(int k) { if (k == 0) throw 11; return deep(k - 1) + 1; }
  constexpr int deep_unwind() {
    try { return deep(50); } catch (int i) { return i; }
  }
  static_assert(deep_unwind() == 11);

  constexpr int locals() {
    int n = 0;
    try {
      Counted a(&n), b(&n);
      throw 0;
    } catch (int) {
      return n;
    }
    return -1;
  }
  static_assert(locals() == 2);

  constexpr int loop_locals() {
    int n = 0;
    for (int i = 0; i < 3; ++i) {
      try {
        Counted c(&n);
        if (i == 1) throw i;
      } catch (int) {
        n += 100;
      }
    }
    return n;
  }
  static_assert(loop_locals() == 103);

  constexpr int exception_object_lifetime() {
    int n = 0;
    try { throw Counted(&n); } catch (Counted&) { if (n != 0) return -1; }
    return n;
  }
  static_assert(exception_object_lifetime() == 1);

  constexpr int catch_by_value() {
    int n = 0;
    try { throw Counted(&n); } catch (Counted c) { if (n != 0) return -1; }
    return n;  // The handler parameter and the exception object.
  }
  static_assert(catch_by_value() == 2);

  constexpr int expr_temps() {
    int n = 0;
    try {
      int x = (Counted(&n), throw 1, 0);
      return x;
    } catch (int) {}
    return n;
  }
  static_assert(expr_temps() == 1);

  struct ThrowingDtor {
    constexpr ~ThrowingDtor() noexcept(false) { throw 2; }
  };
  constexpr int dtor_throws() {
    try { ThrowingDtor t; } catch (int i) { return i; }
    return 0;
  }
  static_assert(dtor_throws() == 2);

  // An exception from the destructor of the exception object propagates if
  // the handler exits normally.
  constexpr int exception_dtor_throws() {
    try {
      try { throw ThrowingDtor(); } catch (...) {}
    } catch (int i) { return i; }
    return 0;
  }
  static_assert(exception_dtor_throws() == 2);

  struct ThrowingCopy {
    constexpr ThrowingCopy() {}
    constexpr ThrowingCopy(const ThrowingCopy&) { throw 3; }
  };
  constexpr int throwing_copy() {
    try {
      ThrowingCopy t;
      throw t;  // The copy throws 3 instead.
    } catch (int i) { return i; }
    return 0;
  }
  static_assert(throwing_copy() == 3);
}

namespace function_try_blocks {
  constexpr int ordinary(int k) try {
    if (k) throw k;
    return 0;
  } catch (int i) {
    return i * 2;
  }
  static_assert(ordinary(3) == 6);
  static_assert(ordinary(0) == 0);

  struct CtorTry {
    Counted a;
    Thrower b;
    constexpr CtorTry(int* p) try : a(p), b(p) {} catch (int) { *p += 100; }
  };
  constexpr int ctor() {
    int n = 0;
    try { CtorTry c(&n); } catch (int i) { return n + i; }  // Rethrown.
    return -1;
  }
  static_assert(ctor() == 106);

  // The handlers of a destructor's function-try-block run after the
  // subobjects are destroyed, and they also apply to their destructions.
  struct Logged {
    int* log;
    int id;
    constexpr ~Logged() noexcept(false) {
      *log = *log * 10 + id;
      if (id == 9) throw id;
    }
  };
  struct DtorTry {
    Logged m;
    constexpr ~DtorTry() noexcept(false) try {
      *m.log = *m.log * 10 + 1;
      throw 2;
    } catch (int& i) {
      i += 1;
    }
  };
  constexpr int dtor() {
    int n = 0;
    try { DtorTry d{{&n, 4}}; } catch (int i) { return n * 10 + i; }
    return -1;                                         // Rethrown.
  }
  static_assert(dtor() == 143);

  struct DtorTryMember {
    Logged m;
    constexpr ~DtorTryMember() try {} catch (int) { return; }
  };
  constexpr int dtor_member() {
    int n = 0;
    { DtorTryMember d{{&n, 9}}; }
    return n;
  }
  static_assert(dtor_member() == 9);
}

namespace partial_construction {
  struct Two {
    Counted a;
    Thrower b;
    constexpr Two(int* p) : a(p), b(p) {}
  };
  constexpr int members() {
    int n = 0;
    try { Two t(&n); } catch (int) { return n; }
    return -1;
  }
  static_assert(members() == 1);

  struct Base {
    int* p;
    constexpr Base(int* p) : p(p) {}
    constexpr ~Base() { ++*p; }
  };
  struct Der : Base {
    Thrower t;
    constexpr Der(int* p) : Base(p), t(p) {}
  };
  constexpr int bases() {
    int n = 0;
    try { Der d(&n); } catch (int) { return n; }
    return -1;
  }
  static_assert(bases() == 1);

  struct Elem {
    int* p;
    constexpr Elem() : p(nullptr) { throw 1; }
    constexpr Elem(int* p, bool t = false) : p(p) { if (t) throw 1; }
    constexpr ~Elem() { if (p) ++*p; }
  };
  constexpr int array() {
    int n = 0;
    try { Elem a[4] = {{&n}, {&n}, {&n, true}, {&n}}; }
    catch (int) { return n; }
    return -1;
  }
  static_assert(array() == 2);

  constexpr int array_default_init() {
    int n = 0;
    try { Elem a[4] = {Elem(&n), Elem(&n)}; } catch (int) { return n; }
    return -1;
  }
  static_assert(array_default_init() == 2);

  struct Agg { Counted a; Counted b; Thrower c; };
  constexpr int aggregate() {
    int n = 0;
    try { Agg g{Counted(&n), Counted(&n), Thrower(&n)}; }
    catch (int) { return n; }
    return -1;
  }
  static_assert(aggregate() == 2);

  constexpr int new_object() {
    int n = 0;
    try { new Thrower(&n); } catch (int) { return 1; }  // Deallocated.
    return -1;
  }
  static_assert(new_object() == 1);

  constexpr int new_array() {
    int n = 0;
    try {
      Elem* p = new Elem[4]{Elem(&n), Elem(&n), Elem(&n)};
      delete[] p;
    } catch (int) {
      return n;
    }
    return -1;
  }
  static_assert(new_array() == 3);

  struct C {
    int* n;
    int k;
    constexpr C(int* n, int k) : n(n), k(k) {}
    constexpr C(const C& o) : n(o.n), k(o.k) { if (k == 2) throw 7; }
    constexpr ~C() { ++*n; }
  };
  struct CA { C a[3]; };
  constexpr int array_copy() {
    int n = 0;
    {
      CA x{{C(&n, 0), C(&n, 1), C(&n, 2)}};
      try {
        CA y = x;
      } catch (int) {
        if (n != 2) return -n;  // Elements 0 and 1 of y were destroyed.
      }
    }
    return n;
  }
  static_assert(array_copy() == 5);

  constexpr int structured_binding_copy() {
    int n = 0;
    {
      C a[3] = {C(&n, 0), C(&n, 1), C(&n, 2)};
      try {
        auto [x, y, z] = a;
      } catch (int) {
        if (n != 2) return -n;  // x and y were destroyed.
      }
    }
    return n;
  }
  static_assert(structured_binding_copy() == 5);

  constexpr int thrower() { throw 6; }
  constexpr int lambda_captures() {
    int n = 0;
    {
      C c1(&n, 1), c2(&n, 2);
      try {
        auto l = [c1, c2] {};
      } catch (int) {
        if (n != 1) return -n;  // The copy of c1 was destroyed.
      }
      try {
        auto l = [c = C(&n, 3), k = thrower()] {};
      } catch (int) {
        if (n != 2) return -n;  // c was destroyed.
      }
    }
    return n;
  }
  static_assert(lambda_captures() == 4);

  struct Logged {
    int* log;
    int id;
    constexpr ~Logged() noexcept(false) {
      *log = *log * 10 + id;
      if (id >= 8) throw id;
    }
  };
  constexpr int delete_array() {
    int n = 0;
    Logged* p = new Logged[3]{{&n, 1}, {&n, 2}, {&n, 3}};
    delete[] p;            // The elements are destroyed in reverse order.
    p = new Logged[3]{{&n, 1}, {&n, 8}, {&n, 3}};
    try { delete[] p; } catch (int i) { return n * 10 + i; }  // Deallocated.
    return -1;
  }
  static_assert(delete_array() == 3213818);

  constexpr int delete_object() {
    int n = 0;
    Logged* p = new Logged{&n, 9};
    try { delete p; } catch (int i) { return n * 10 + i; }  // Deallocated.
    return -1;
  }
  static_assert(delete_object() == 99);

  // A lifetime-extended temporary whose initialization throws is not
  // destroyed.
  struct LT {
    int* log;
    constexpr LT(int* l, bool t) : log(l) { if (t) throw 1; }
    constexpr ~LT() { ++*log; }
  };
  constexpr int extended_temp() {
    int n = 0;
    { const LT& r = LT(&n, false); }
    try { const LT& r = LT(&n, true); } catch (int) {}
    return n;
  }
  static_assert(extended_temp() == 1);
}

namespace ignored_evaluations {
  // An exception thrown by an assumption expression (which is not evaluated
  // at run time) does not propagate.
  constexpr int assume_throws() {
    try { [[assume((throw 1, true))]]; return 1; } catch (int) { return 2; }
  }
  static_assert(assume_throws() == 1);
  constexpr int assume_rethrows() {
    try {
      throw 5;
    } catch (int i) {
      [[assume((throw, true))]];
      return i;
    }
  }
  static_assert(assume_rethrows() == 5);

  // The same applies if the evaluation fails because std::terminate is
  // called, or for another reason while an exception is being handled.
  constexpr int terminates() noexcept { throw 1; }
  int nonconstexpr;
  constexpr bool fails_in_handler() {
    try { throw; } catch (int) { return nonconstexpr == 0; }
    return true;
  }
  constexpr int assume_fails() {
    [[assume(terminates() == 0)]];
    try {
      throw 6;
    } catch (int i) {
      [[assume(fails_in_handler())]];
      return i;
    }
  }
  static_assert(assume_fails() == 6);
}

namespace handler_mismatches {
  // Handlers that a thrown exception does not match.
  struct B { int v = 1; };
  struct PrivD : private B {};
  struct ProtD : protected B {};
  struct L : B {};
  struct R : B {};
  struct Amb : L, R {};
  struct PubD : B { int w = 2; };

  template<class T> constexpr int by_ref() {
    try { throw T(); } catch (B&) { return 1; } catch (...) { return 2; }
  }
  static_assert(by_ref<PubD>() == 1);
  static_assert(by_ref<PrivD>() == 2);
  static_assert(by_ref<ProtD>() == 2);
  static_assert(by_ref<Amb>() == 2);

  template<class T> constexpr int by_ptr() {
    T *p = nullptr;
    try { throw p; } catch (const B*) { return 1; } catch (...) { return 2; }
  }
  static_assert(by_ptr<PubD>() == 1);
  static_assert(by_ptr<PrivD>() == 2);
  static_assert(by_ptr<ProtD>() == 2);
  static_assert(by_ptr<Amb>() == 2);

  // Pointer-to-member conversions are not applied, in either direction, but
  // a null pointer constant of type std::nullptr_t matches.
  constexpr int base_member() {
    int B::*pm = &B::v;
    try { throw pm; } catch (int PubD::*) { return 1; }
    catch (...) { return 2; }
  }
  static_assert(base_member() == 2);
  constexpr int derived_member() {
    int PubD::*pm = &PubD::w;
    try { throw pm; } catch (int B::*) { return 1; } catch (...) { return 2; }
  }
  static_assert(derived_member() == 2);
  constexpr int null_member() {
    try {
      throw nullptr;
    } catch (int PubD::*pm) {
      return pm == nullptr;
    } catch (...) {
      return 2;
    }
  }
  static_assert(null_member() == 1);

  // Only qualification conversions that are valid implicit conversions apply.
  constexpr int qualification() {
    int *p = nullptr;
    int **pp = &p;
    try { throw pp; } catch (const int**) { return 1; }
    catch (...) { return 2; }
  }
  static_assert(qualification() == 2);
}
