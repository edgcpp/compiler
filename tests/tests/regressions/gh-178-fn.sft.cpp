//type:fn
//options_all:--c++17 --max_depth_constexpr_call=300

// Companion to gh-178.sft.cpp: a call in an argument no longer counts as
// nested in the call it is passed to, but the depth limit is still enforced.

constexpr int add(int a, int b) { return a + b; }
constexpr int f(int n) { return n == 0 ? 0 : add(1, f(n - 1)); }

// 301 nested calls of f: one call too many, still beyond the limit.
static_assert(f(300) == 300, "");
