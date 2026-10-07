//type:fp
//use_system_includes: true
//edg_header_pack: exp_meta
//require:DO_IL_LOWERING 1

/*
std::meta::type_of of a nonstatic member function is a plain function type
([dcl.fct]): the class is not part of it, but the cv-qualifiers, the
ref-qualifier, and the exception specification are.  So it compares equal to
the reflection of that function type, and the spliced type matches partial
specializations for function types.
*/

#include <experimental/meta>

using namespace std::meta;

struct M {
  int f(int, double);
  void g() const noexcept;
  void h() volatile &&;
  static int s(int);
  void d(int = 1);
  void e(this M &, int);
  virtual void v();
};

static_assert(type_of(^^M::f) == ^^int(int, double));
static_assert(type_of(^^M::g) == ^^void() const noexcept);
static_assert(type_of(^^M::h) == ^^void() volatile &&);
static_assert(type_of(^^M::s) == ^^int(int));
static_assert(type_of(^^M::d) == ^^void(int));
static_assert(type_of(^^M::e) == ^^void(M &, int));
static_assert(type_of(^^M::v) == ^^void());
static_assert(is_function_type(type_of(^^M::f)));

template <typename> struct Kind { static constexpr int value = 0; };
template <typename R, typename... A> struct Kind<R(A...)> {
  static constexpr int value = 1;
};
template <typename R, typename... A> struct Kind<R(A...) const noexcept> {
  static constexpr int value = 2;
};
template <typename R, typename... A> struct Kind<R(A...) volatile &&> {
  static constexpr int value = 3;
};

static_assert(Kind<typename[:type_of(^^M::f):]>::value == 1);
static_assert(Kind<typename[:type_of(^^M::g):]>::value == 2);
static_assert(Kind<typename[:type_of(^^M::h):]>::value == 3);

// The function type gives back the pointer-to-member type.
using F = [:type_of(^^M::f):];
using G = [:type_of(^^M::g):];
using PF = F M::*;
using PG = G M::*;
static_assert(dealias(^^PF) == ^^int (M::*)(int, double));
static_assert(dealias(^^PG) == ^^void (M::*)() const noexcept);

// The result does not depend on where type_of is evaluated.
consteval info type_of_g() { return type_of(^^M::g); }
constexpr info g_type = type_of_g();
static_assert(g_type == type_of(^^M::g));
