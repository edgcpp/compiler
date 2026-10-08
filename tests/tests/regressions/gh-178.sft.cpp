//type:fp
//options_all:--c++17 --max_depth_constexpr_call=300

// A constexpr call that appears in an argument of another constexpr call is
// evaluated before that other call is entered, so it is not nested in it.
// Previously, the interpreter charged the outer call against the call depth
// limit before evaluating its arguments, so each level of recursion below
// counted twice and only about half the requested depth was available.

constexpr int add(int a, int b) { return a + b; }
constexpr int f(int n) { return n == 0 ? 0 : add(1, f(n - 1)); }
static_assert(f(256) == 256, "");   // Previously not a constant.
static_assert(f(299) == 299, "");   // 300 nested calls of f: at the limit.

struct S {
  int v;
  constexpr S(int x) : v(x) {}
};
constexpr int g(int n) { return n == 0 ? 0 : S(1 + g(n - 1)).v; }
static_assert(g(299) == 299, "");   // Previously not a constant.
