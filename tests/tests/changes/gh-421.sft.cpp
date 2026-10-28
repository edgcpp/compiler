//type:fp
//options:--c++20
//remark:Constant evaluation: overrider below a class that does not override

struct A {
  constexpr virtual int f() const { return 1; }
  int a = 0;
};
struct NA : A {};
struct B : NA {
  constexpr int f() const override { return 2; }
};
constexpr int g1() { B b; const A* p = &b; return p->f(); }
static_assert(g1() == 2);  // Previously failed.
constexpr int g1n() { B b; const NA* p = &b; return p->f(); }
static_assert(g1n() == 2);  // Previously failed.

// Two non-overriding levels.
struct NNA : NA {};
struct C : NNA { constexpr int f() const override { return 3; } };
constexpr int g2() { C c; const A* p = &c; return p->f(); }
static_assert(g2() == 3);  // Previously failed.

// The overrider is in a middle class, above a non-overriding one.
struct M : NA { constexpr int f() const override { return 4 + a; } };
struct NM : M {};
struct E { int e = 7; };
struct T : E, NM {};
constexpr int g3() { T t; t.a = 10; const A* p = &t; return p->f(); }
static_assert(g3() == 14);  // Previously failed.

// Repeated base class: only one path overrides.
struct L : A { constexpr int f() const override { return 5 + a; } };
struct R : A {};
struct D : L, R {};
constexpr int g4() {
  D d;
  static_cast<L&>(d).a = 1;
  static_cast<R&>(d).a = 2;
  const A* pl = static_cast<const L*>(&d);
  const A* pr = static_cast<const R*>(&d);
  return pl->f() * 10 + pr->f();
}
static_assert(g4() == 61);
struct DD : D {
  constexpr int f() const override { return 9; }
};
constexpr int g5() {
  DD d;
  const A* pl = static_cast<const L*>(&d);
  const A* pr = static_cast<const R*>(&d);
  return pl->f() * 10 + pr->f();
}
static_assert(g5() == 99);  // Previously failed.

// Covariant return through a non-overriding intermediate class.
struct X { int x = 'X'; };
struct Y : X { int y = 'Y'; };
struct Z0 { int z = 0; };
struct Q : Z0, Y { int q = 'Q'; };
struct CA { constexpr virtual const X* h() const { return &xx; } X xx; };
struct CN : CA {};
struct CB : CN {
  constexpr const Q* h() const override { return &qq; }
  Q qq;
};
constexpr bool g6() {
  CB b;
  const CA* p = &b;
  return p->h() == static_cast<const X*>(&b.qq) && p->h()->x == 'X';
}
static_assert(g6());  // Previously failed.

// Virtual call during construction of an intermediate class.
struct K : NA {
  int r;
  constexpr K() : r(static_cast<const A*>(this)->f()) {}
  constexpr int f() const override { return 6; }
};
struct NK : K {};
struct KK : NK { constexpr int f() const override { return 8; } };
constexpr int g7() { KK k; const A* p = &k; return k.r * 10 + p->f(); }
static_assert(g7() == 68);  // Previously failed.
struct W : NA {
  int r;
  constexpr W() : r(static_cast<const A*>(this)->f()) {}
};
struct WW : W { constexpr int f() const override { return 8; } };
constexpr int g8() { WW w; const A* p = &w; return w.r * 10 + p->f(); }
static_assert(g8() == 18);  // Previously failed.

constexpr B gb;
static_assert(static_cast<const A&>(gb).f() == 2);
