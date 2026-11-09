//type:fp
//options:--c++20
//remark:[GH #397] Constant evaluation of a virtual call whose final overrider
//remark:is a member of an intermediate base class

struct A { constexpr virtual int f() const { return 1; } int a = 1; };
struct B : A { constexpr int f() const override { return a + 2; } };
struct E { int e = 0; };
struct F : E, B {};
constexpr int g() { F f; A& r = f; return r.f(); }
static_assert(g() == 3);  // Previously failed.

struct G : B {};
constexpr int h() { G g; A& r = g; return r.f(); }
static_assert(h() == 3);  // Previously failed.

struct H : E, G { int x = 5; };
constexpr int i() { H h; A& ra = h; B& rb = h; return ra.f() * 10 + rb.f(); }
static_assert(i() == 33);  // Previously failed.

struct C : A {};
constexpr int j() { C c; A& r = c; return r.f(); }
static_assert(j() == 1);

struct X { int x = 7; };
struct Y { int y = 0; };
struct Z : Y, X {};
struct P { constexpr virtual const X* p() const { return nullptr; } };
struct Q : P { Z z; constexpr const Z* p() const override { return &z; } };
struct R : E, Q {};
constexpr int k() { R r; const P& rp = r; return rp.p()->x; }
static_assert(k() == 7);
struct S : E, P {
  Z z;
  bool null;
  constexpr S(bool b) : null(b) {}
  constexpr const Z* p() const override { return null ? nullptr : &z; }
};
constexpr bool l(bool b) {
  S s(b);
  const P& rp = s;
  return b ? rp.p() == nullptr : rp.p() == (const X*)&s.z;
}
static_assert(l(false));  // Previously failed.
static_assert(l(true));
struct W : Y, Z {};
struct T : Q { W w; constexpr const W* p() const override { return &w; } };
struct U : E, T {};
constexpr bool m() {
  U u;
  const P& rp = u;
  const Q& rq = u;
  return rp.p() == (const X*)&u.w && rq.p() == (const Z*)&u.w &&
         rp.p()->x == 7;
}
static_assert(m());  // Previously failed.

// A covariant return type whose class is an indirect base class.
struct V : Y, Z {};
struct VV : E, V {};
struct I : P { VV vv; constexpr const VV* p() const override { return &vv; } };
struct J : E, I {};
constexpr bool n() {
  J j;
  const P& rp = j;
  return rp.p() == (const X*)&j.vv && rp.p()->x == 7;
}
static_assert(n());
