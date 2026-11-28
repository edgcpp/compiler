//type:rp
//options:--c++11 --g++ --gnu_version=90000:--c++20 --g++ --gnu_version=150000:--c++20 --clang_version 230100:--c++14 --g++ --gnu_version=80000
// The GNU two-operand conditional "x ? : y" reuses the value of its first
// operand.  When that operand is an xvalue, such as "S().s" (Core issue 616)
// or "id<S[2]>{...}[i].s" (Core issue 1213), the front end used to fail an
// assertion while saving it for reuse.  Each failing check prints its line
// number.
extern "C" int printf(const char *, ...);
int fails;
void check_(bool b, int line) {
  if (!b) {
    printf("check on line %d failed\n", line);
    ++fails;
  }
}
#define check(b) check_(b, __LINE__)

template <typename T> using id = T;
struct S {
  S() : s(false) {}
  S(bool x) : s(x) {}
  bool s;
};
S g(true);

bool member(bool x) {
  const bool &r = S(x).s ? : S(true).s;
  return r;
}

bool subscript(int i) {
  const bool &r = id<S[2]>{false, true}[i].s ? : id<S[2]>{false, false}[i].s;
  return r;
}

bool cast(bool x) {
  g.s = x;
  bool &&r = static_cast<S &&>(g).s ? : static_cast<bool &&>(g.s);
  return r;
}

int main() {
  check(member(true));
  check(member(false));
  check(subscript(1));
  check(!subscript(0));
  check(cast(true));
  check(!cast(false));
  return fails;
}
