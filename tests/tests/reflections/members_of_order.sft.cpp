//type:fp
//use_system_includes: true
//edg_header_pack: exp_meta
//require:DO_IL_LOWERING 1

/*
std::meta::members_of returns the members of a class in the order in which
they are declared, whatever their kinds (N5046
[meta.reflection.member.queries]); implicitly-declared special members come
after the user-declared members.  Members of a namespace are also returned in
declaration order.  The order is that of the first declarations: a member
defined outside its class or namespace, or declared by a macro expansion, is
placed where it is declared.
*/

#include <experimental/meta>
#include <string>
#include <string_view>

using namespace std::meta;

// The names of the members of r in the order of members_of, except for
// implicitly-declared special members (how many of them are declared is not
// checked here).  A member that follows an implicitly-declared one is marked.
consteval std::string names(info r, access_context ctx) {
  std::string out;
  bool        after_implicit = false;
  for (info m : members_of(r, ctx)) {
    if (is_special_member_function(m) && !is_user_declared(m)) {
      after_implicit = true;
      continue;
    }  /* if */
    if (after_implicit) out += "<after implicit>";
    out += has_identifier(m) ? identifier_of(m) : "<unnamed>";
    out += ' ';
  }  /* for */
  return out;
}

consteval int implicit_count(info r) {
  int n = 0;
  for (info m : members_of(r, access_context::unchecked())) {
    n += is_special_member_function(m) && !is_user_declared(m);
  }  /* for */
  return n;
}

consteval std::string names(info r) {
  return names(r, access_context::unchecked());
}

struct S {
  int a;
  void f();
  int b;
  static void g();
  using T = int;
  int c;
  struct N {};
  static int sv;
  enum E { e };
  int : 3;
  S() = default;
  int d;
};

static_assert(names(^^S) == "a f b g T c N sv E <unnamed> S d ");
static_assert(implicit_count(^^S) != 0);

// Members declared in the class and defined outside it are placed where they
// are declared, not where they are defined.
struct O {
  void f();
  int x;
  static int s;
  struct N;
  int y;
  O();
  int z;
};
void O::f() {}
int O::s;
struct O::N {};
O::O() {}

static_assert(names(^^O) == "f x s N y O z ");

// Members declared by one macro expansion are in the order of the expansion,
// although they all have the position of the macro invocation.
#define MEMBERS(n) int m1##n; void g##n(); int m2##n; using T##n = int;
struct M {
  int a;
  MEMBERS(1) MEMBERS(2)
  int b;
};

static_assert(names(^^M) == "a m11 g1 m21 T1 m12 g2 m22 T2 b ");

// An unnamed bit-field is placed after the member declared just before it,
// whatever the kind of that member.
struct B {
  int a : 2;
  void h();
  int : 3;
  int b : 2;
  using U = int;
  int : 1;
  int c : 2;
};

static_assert(names(^^B) == "a h <unnamed> b U <unnamed> c ");

class P {
  int p1;
public:
  int q1;
  void q2();
private:
  int p2;
public:
  using q3 = int;
};

static_assert(names(^^P, access_context::current()) == "q1 q2 q3 ");

// Members of an instantiation follow the declarations in the template, also
// when a member is defined outside the template.
template <typename U> struct TS {
  U x; void h(); U y;
  static U z;
  void k();
  U w;
};
template <typename U> void TS<U>::k() {}

static_assert(names(^^TS<int>) == "x h y z k w ");
static_assert(implicit_count(^^TS<int>) != 0);

// Members of a class defined by define_aggregate.
struct Agg;
consteval {
  define_aggregate(^^Agg, {data_member_spec(^^int, {.name = "m1"}),
                           data_member_spec(^^char, {.name = "m2"})});
}

static_assert(names(^^Agg) == "m1 m2 ");
static_assert(implicit_count(^^Agg) != 0);

namespace ns {
  void f();
  int v;
  struct C;
  namespace inner {}
  using A = int;
  namespace alias = inner;
}
void ns::f() {}
struct ns::C {};
namespace ns {
  void g();
  int w;
}
void ns::g() {}

static_assert(names(^^ns) == "f v C inner A alias g w ");
