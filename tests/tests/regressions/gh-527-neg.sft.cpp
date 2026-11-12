//type:fn
//options_all:--c++26
//remark:Non-constexpr constructor call with a non-throwing argument

constexpr int h(int n) { if (n < 0) throw n; return n; }

struct S {
  S(int) {}
  constexpr S(double) {}
};

constexpr int f(int n) {
  try { S(h(n)); } catch (int i) { return i; }
  return 0;
}
static_assert(f(-1) == -1);
static_assert(f(1) == 0);  // Error: S::S(int) is not constexpr.

// An argument that is not a constant is diagnosed first.
constexpr int g() {
  int* p = nullptr;
  S(h(*p));
  return 0;
}
static_assert(g() == 0);  // Error: *p is not a constant.
