//type:fp
//options:--c++20
//remark:Constant evaluation: range-based for variable destroyed each iteration

struct R { int* p; constexpr ~R() { ++*p; } };
constexpr int simple() {
  int n = 0;
  R a[2] = {{&n}, {&n}};
  for (R r : a) {}
  return n;
}
static_assert(simple() == 2);  // Previously failed.

struct S {
  int* p;
  constexpr S(int* p) : p(p) { *p = *p * 10 + 1; }
  constexpr S(const S& s) : p(s.p) { *p = *p * 10 + 1; }
  constexpr ~S() { *p = *p * 10 + 2; }
};

constexpr int order() {
  int n = 0;
  S a[2] = {S(&n), S(&n)};
  n = 0;
  for (S s : a) n = n * 10 + 3;
  return n;
}
static_assert(order() == 132132);  // Previously failed.

constexpr int with_break() {
  int n = 0;
  S a[2] = {S(&n), S(&n)};
  n = 0;
  for (S s : a) { n = n * 10 + 3; break; }
  return n;
}
static_assert(with_break() == 132);  // Previously failed.

constexpr int with_continue() {
  int n = 0;
  S a[2] = {S(&n), S(&n)};
  n = 0;
  bool first = true;
  for (S s : a) {
    if (first) { first = false; n = n * 10 + 4; continue; }
    n = n * 10 + 3;
  }
  return n;
}
static_assert(with_continue() == 142132);  // Previously failed.

constexpr int by_reference() {
  int n = 0;
  S a[2] = {S(&n), S(&n)};
  n = 0;
  for (const S& s : a) n = n * 10 + 3;
  return n;
}
static_assert(by_reference() == 33);
