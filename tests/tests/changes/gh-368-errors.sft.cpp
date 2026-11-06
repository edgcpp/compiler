//type:fn
//options_all:--c++26
//remark:P3068R6: Invalid exception handling during constant evaluation

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
  };
  class bad_cast : public exception {
  public:
    constexpr bad_cast() noexcept {}
  };
  class bad_typeid : public exception {
  public:
    constexpr bad_typeid() noexcept {}
  };
  class bad_alloc : public exception {
  public:
    constexpr bad_alloc() noexcept {}
  };
  class bad_array_new_length : public bad_alloc {
  public:
    constexpr bad_array_new_length() noexcept {}
  };
}

struct E { int v; };
constexpr int uncaught() { throw E{1}; return 0; }
static_assert(uncaught() == 0);              // Uncaught E.

constexpr int wrong_handler() {
  try { throw 1; } catch (long) { return 1; }
  return 0;
}
static_assert(wrong_handler() == 1);         // Uncaught int.

constexpr int bare_rethrow() { throw; return 0; }
static_assert(bare_rethrow() == 0);          // Nothing being handled.

constexpr void nt() noexcept { throw 1; }
constexpr int noexcept_boundary() {
  try { nt(); } catch (int) { return 1; }
  return 0;
}
static_assert(noexcept_boundary() == 1);     // std::terminate.

struct Dtor { constexpr ~Dtor() { throw 1; } };
constexpr int implicit_noexcept_dtor() {
  try { Dtor d; } catch (int) { return 1; }
  return 0;
}
static_assert(implicit_noexcept_dtor() == 1);  // std::terminate.

struct ThrowingDtor {
  constexpr ~ThrowingDtor() noexcept(false) { throw 2; }
};
constexpr int during_unwinding() {
  try { ThrowingDtor t; throw 1; } catch (...) { return 1; }
  return 0;
}
static_assert(during_unwinding() == 1);      // std::terminate.

struct Logged {
  int id;
  constexpr ~Logged() noexcept(false) { if (id > 0) throw id; }
};
constexpr int delete_array() {
  Logged* p = new Logged[3]{{1}, {0}, {2}};
  try { delete[] p; } catch (int) { return 1; }
  return 0;
}
static_assert(delete_array() == 1);          // std::terminate (element 0).

struct CtorTry {
  int v;
  constexpr CtorTry() try : v(1) { throw 1; }
  catch (int) { (void)(v + 1); }
};
constexpr int ctor_handler_member() {
  try { CtorTry c; } catch (int) { return 1; }
  return 0;
}
static_assert(ctor_handler_member() == 1);   // v accessed after destruction.

struct ThrowingCopy {
  constexpr ThrowingCopy() {}
  constexpr ThrowingCopy(const ThrowingCopy&) { throw 3; }
};
constexpr int handler_init() {
  try { throw ThrowingCopy(); } catch (ThrowingCopy) { return 1; }
  return 0;
}
static_assert(handler_init() == 1);          // std::terminate.

struct B { virtual ~B() = default; };
struct D1 : B {};
struct D2 : B {};
constexpr int bad_cast() {
  D1 d;
  B& b = d;
  (void)dynamic_cast<D2&>(b);
  return 0;
}
static_assert(bad_cast() == 0);              // Uncaught std::bad_cast.

constexpr int bad_typeid(B* p) {
  (void)typeid(*p);
  return 0;
}
static_assert(bad_typeid(nullptr) == 0);     // Uncaught std::bad_typeid.

constexpr int bad_array_new_length(int n) {
  int* p = new int[n];
  delete[] p;
  return 0;
}
static_assert(bad_array_new_length(-2) == 0);  // Uncaught
                                               // std::bad_array_new_length.

int nonconstexpr();
constexpr int failed_assumption() {
  // The failure to evaluate the assumption expression is not reported.
  [[assume(nonconstexpr() == 1)]];
  int* p = nullptr;
  return *p;
}
static_assert(failed_assumption() == 0);     // Null pointer dereference.

constexpr void* operator new(decltype(sizeof(0)), void* p) noexcept {
  return p;
}
void operator delete(void*, void*) noexcept;
namespace std {
  template<class T> constexpr T* construct_at(T* p) noexcept {
    return ::new (static_cast<void*>(p)) T();
  }
}
struct ThrowingCtor { constexpr ThrowingCtor() { throw 1; } };
constexpr int noexcept_construct_at() {
  union U { char c; ThrowingCtor t; } u{'a'};
  try { std::construct_at(&u.t); } catch (int) { return 1; }
  return 0;
}
static_assert(noexcept_construct_at() == 1);  // std::terminate.
