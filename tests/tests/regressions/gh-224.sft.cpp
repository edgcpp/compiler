//type:fp
//options:--c++20 --g++ --gnu_version=150200:--c++20 --clang --clang_version=220100
//remark:Constant evaluation of GNU vector objects and their elements.

// The interpreter did not reserve an initialization bitmap for complete
// objects of GNU vector type, so tracking the initialization of their elements
// wrote past the object's bookkeeping prefix (a 64-byte vector suffices).
// Assigning a whole vector subobject also marked only its first element as
// initialized.

typedef char vc64 __attribute__((vector_size(64)));
typedef int vi4 __attribute__((vector_size(16)));
typedef long long vl8 __attribute__((vector_size(64)));

int main() {
  vc64 a = vc64();
  return (int)a[0];
}

constexpr int f1() { vc64 a = vc64(); return a[63]; }
static_assert(f1() == 0, "");

constexpr int f2() { vc64 a{}; return a[0] + a[63]; }
static_assert(f2() == 0, "");

constexpr int f3() { vl8 a = vl8(); vl8 b = a; return (int)b[7]; }
static_assert(f3() == 0, "");

constexpr int f4() {
  vl8 a = vl8();
  a[7] = 3;
  vl8 b;
  b = a;
  return (int)b[7];
}
static_assert(f4() == 3, "");

constexpr vc64 g5() { return vc64(); }
constexpr int f5() { return g5()[63]; }
static_assert(f5() == 0, "");

constexpr vl8 v6 = vl8();
static_assert(v6[7] == 0, "");

struct S { vl8 v; };
struct T { vi4 v; int i; };

constexpr int f7() { S s{}; return (int)s.v[7]; }
static_assert(f7() == 0, "");

constexpr int f8() { S s; s.v = vl8(); return (int)s.v[7]; }
static_assert(f8() == 0, "");

constexpr int f9() { T t; t.v = vi4{1, 2, 3, 4}; t.i = 0; return t.v[3]; }
static_assert(f9() == 4, "");

constexpr int f10() { vi4 a[2]; a[1] = vi4{1, 2, 3, 4}; return a[1][3]; }
static_assert(f10() == 4, "");
