//type:fp
//options_all:--c++20

// Zero-initialization makes the first named member of each union subobject
// active ([dcl.init.general]).  Previously, when a value-initialized temporary
// was assigned to an object during constant evaluation (x = R()), the union
// subobjects of the temporary were left with no active member, so reading
// the first member afterwards was not a constant.

struct R {
  union { int a; long b; };
};
constexpr int f() {
  R x;
  x = R();
  return x.a;
}
static_assert(f() == 0);          // Previously not a constant.

union U { int a; long b; };
struct N { U u; };
constexpr int g() {
  N x;
  x = N();
  return x.u.a;
}
static_assert(g() == 0);          // Previously not a constant.

struct A { U arr[2]; };
constexpr int h() {
  A x;
  x = A();
  return x.arr[1].a;
}
static_assert(h() == 0);          // Previously not a constant.

// A union nested in a union, in a base class subobject.
union V { union { int p; } in; long q; };
struct B { V v; };
struct D : B { int z; };
constexpr int k() {
  D x;
  x = D();
  return x.v.in.p + x.z;
}
static_assert(k() == 0);          // Previously not a constant.

// Forms that were already accepted.
constexpr int l1() { R x; x = R{}; return x.a; }
static_assert(l1() == 0);
constexpr int l2() { R x = R(); return x.a; }
static_assert(l2() == 0);
