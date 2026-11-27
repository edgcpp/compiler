//type:fn
//options_all:--c++20

// Companion to constexpr-union-zero-init-assign.sft.cpp: after x = R(), the
// first union member is active, so reading another member is still not a
// constant.  The note names the active member.

struct R {
  union { int a; long b; };
};
constexpr int f() {
  R x;
  x = R();
  return x.b;
}
static_assert(f() == 0);
