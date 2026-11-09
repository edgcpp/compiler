//type:fp
//options:--c++20
//remark:dynamic_cast in constant evaluation requires public base subobjects

// The run-time checks require public base class subobjects, independent of
// access in the context of the cast.
struct B { virtual void b() {} };
struct C { virtual void c() {} };
struct D : private B {
  virtual void d() {}
  constexpr const B* pb() const { return this; }
};
struct E { virtual void e() {} };
struct F : D, E, private C {
  virtual void f() {}
  constexpr bool m() const {
    return dynamic_cast<const F*>((const C*)this) == nullptr &&
           dynamic_cast<const E*>((const C*)this) == nullptr;
  }
};

constexpr F gf;
// Each of the following three assertions previously failed.
static_assert(dynamic_cast<const D*>((const B*)&gf) == nullptr);
static_assert(dynamic_cast<const E*>((const B*)&gf) == nullptr);
static_assert(dynamic_cast<const F*>((const C*)&gf) == nullptr);
static_assert(dynamic_cast<const F*>((const D*)&gf) == &gf);
static_assert(dynamic_cast<const F*>((const E*)&gf) == &gf);
static_assert(dynamic_cast<const E*>((const D*)&gf) == &gf);
static_assert(dynamic_cast<const void*>((const B*)&gf) == &gf);
static_assert(gf.m());  // Previously failed.

constexpr bool local() {
  F f;
  const B* pb = (const B*)&f;
  const C* pc = (const C*)&f;
  return dynamic_cast<const D*>(pb) == nullptr &&
         dynamic_cast<const E*>(pb) == nullptr &&
         dynamic_cast<const F*>(pc) == nullptr &&
         dynamic_cast<const E*>(pc) == nullptr &&
         dynamic_cast<const D*>(f.pb()) == nullptr &&
         dynamic_cast<const F*>((const D*)&f) == &f &&
         dynamic_cast<const E*>((const D*)&f) == &f &&
         dynamic_cast<const D*>((const E*)&f) == &f &&
         dynamic_cast<const void*>(pb) == &f &&
         f.m();
}
static_assert(local());  // Previously failed.

struct G : B {};
struct H : G, E {};
constexpr bool public_bases() {
  H h;
  const B* pb = &h;
  return dynamic_cast<const G*>(pb) == &h &&
         dynamic_cast<const H*>(pb) == &h &&
         dynamic_cast<const E*>(pb) == &h &&
         dynamic_cast<const B*>((const E*)&h) == pb;
}
static_assert(public_bases());

// A downcast only requires the operand to be a public base class subobject
// of the destination object, not of the most-derived object.
struct K : B {};
struct L : K {};
struct M : private L, E {};
constexpr M gm;
static_assert(dynamic_cast<const K*>((const B*)&gm) == (const K*)&gm);
static_assert(dynamic_cast<const L*>((const B*)&gm) == (const L*)&gm);
static_assert(dynamic_cast<const M*>((const B*)&gm) == nullptr);
static_assert(dynamic_cast<const E*>((const B*)&gm) == nullptr);
constexpr bool private_most_derived() {
  M m;
  const B* pb = (const B*)&m;
  return dynamic_cast<const K*>((const B*)&m) == (const K*)&m &&
         dynamic_cast<const L*>((const B*)&m) == (const L*)&m &&
         dynamic_cast<const L*>(pb) == (const L*)&m &&
         dynamic_cast<const M*>(pb) == nullptr &&
         dynamic_cast<const E*>(pb) == nullptr;
}
static_assert(private_most_derived());
