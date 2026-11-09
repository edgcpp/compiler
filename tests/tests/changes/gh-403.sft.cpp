//type:fp
//options:--c++20
//remark:Constant evaluation: destruction point of condition variables

struct S {
  int* p;
  int* cnt;
  constexpr S(int* p, int* cnt) : p(p), cnt(cnt) { *p = *p * 10 + 1; }
  constexpr S(const S& s) : p(s.p), cnt(s.cnt) { *p = *p * 10 + 1; }
  constexpr ~S() { *p = *p * 10 + 2; }
  constexpr explicit operator bool() const { return (*cnt)-- > 0; }
  constexpr operator int() const { return *cnt; }
};

constexpr int if_cond() {
  int n = 0, cnt = 1;
  if (S s{&n, &cnt}) n = n * 10 + 3;
  return n;
}
static_assert(if_cond() == 132);  // Previously failed.

constexpr int if_cond_else() {
  int n = 0, cnt = 0;
  if (S s{&n, &cnt}) n = n * 10 + 3; else n = n * 10 + 4;
  return n;
}
static_assert(if_cond_else() == 142);  // Previously failed.

constexpr int if_init() {
  int n = 0, cnt = 0;
  if (S s{&n, &cnt}; true) n = n * 10 + 3;
  return n;
}
static_assert(if_init() == 132);  // Previously failed.

constexpr int switch_cond() {
  int n = 0, cnt = 4;
  switch (S s{&n, &cnt}) { case 4: n = n * 10 + 3; break; }
  return n;
}
static_assert(switch_cond() == 132);  // Previously failed.

constexpr int while_cond() {
  int n = 0, cnt = 1;
  while (S s{&n, &cnt}) n = n * 10 + 3;
  return n;
}
static_assert(while_cond() == 13212);  // Previously failed.

constexpr int for_cond() {
  int n = 0, cnt = 2;
  for (int i = 0; S s{&n, &cnt}; n = n * 10 + 4) {
    if (i++ == 0) continue;
    n = n * 10 + 3;
  }
  return n;
}
static_assert(for_cond() == 142134212);  // Previously failed.

constexpr int while_break() {
  int n = 0, cnt = 5;
  while (S s{&n, &cnt}) { n = n * 10 + 3; break; }
  return n;
}
static_assert(while_break() == 132);  // Previously failed.

constexpr int while_return(int* p) {
  int cnt = 5;
  while (S s{p, &cnt}) return *p;
  return -1;
}
constexpr int while_return() {
  int n = 0;
  int r = while_return(&n);
  return r * 100 + n;
}
static_assert(while_return() == 112);  // Previously failed.
