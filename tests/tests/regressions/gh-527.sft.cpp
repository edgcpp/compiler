//type:fp
//options_all:--c++26
//remark:Non-constexpr constructor call whose argument throws in constexpr

// The arguments of a constructor call are evaluated before the constructor is
// invoked: If an argument exits by throwing an exception, the constructor is
// not invoked, so it doesn't matter that it isn't constexpr.
constexpr int h(int n) { throw n; }

struct S {
  S(int) {}
  constexpr S(double) {}
};

constexpr int temporary() {
  try { S(h(1)); } catch (int i) { return i; }
  return 0;
}
static_assert(temporary() == 1);

constexpr int variable() {
  try { S s(h(2)); } catch (int i) { return i; }
  return 0;
}
static_assert(variable() == 2);

constexpr int new_expression() {
  try { delete new S(h(3)); } catch (int i) { return i; }
  return 0;
}
static_assert(new_expression() == 3);

struct M {
  S s;
  constexpr M() : s(h(4)) {}
};
constexpr int member() {
  try { M m; } catch (int i) { return i; }
  return 0;
}
static_assert(member() == 4);

struct D : S {
  constexpr D() : S(h(5)) {}
};
constexpr int base() {
  try { D d; } catch (int i) { return i; }
  return 0;
}
static_assert(base() == 5);

// The arguments evaluated before the one that throws are destroyed.
struct L {
  int* p;
  constexpr L(int* p) : p(p) {}
  constexpr ~L() { ++*p; }
};
struct T {
  T(const L&, int) {}
  constexpr T(double) {}
};
constexpr int destroyed() {
  int n = 0;
  try { T(L(&n), h(6)); } catch (int i) { return n * 10 + i; }
  return 0;
}
static_assert(destroyed() == 16);
