//type:fp
//options:--c++23
//remark:[GH #393] [[assume]] with an inlinable call in its argument
//
// Lowering inlined a call in an [[assume]] argument by turning the empty
// statement that carries the attribute into a block, which lost the
// attribute.  The IL write/read check then failed with "not all expected
// entries were read" for the scoped-expression entry of the attribute
// argument.

int nc();
constexpr bool g(bool b) { return b; }
constexpr bool g1() { return nc() > 0; }
constexpr bool g2(bool b) { if (!b) throw 1; return b; }
inline bool g3(bool b) { return b; }

constexpr int f(bool b) { [[assume(g(b))]]; return 1; }
constexpr int f1() { [[assume(g1())]]; return 1; }
constexpr int f2(bool b) { [[assume(g2(b))]]; return 1; }
int f3(bool b) { [[assume(g3(b))]]; return 1; }
consteval int f4(bool b) { [[assume(g(b))]]; return 1; }
constexpr int f5(bool b) { [[assume(g(b))]]; [[assume(g(b))]]; return 1; }

int use(bool b) {
  return f(b) + f1() + f2(b) + f3(b) + f4(true) + f5(b);
}
