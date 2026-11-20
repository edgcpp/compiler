//type:rp
//options_all:--c++17 --exceptions

// Value-/brace-initialization of an aggregate whose array member has a
// repeated initializer for a class with a destructor (e.g., "G g{}" where
// G has a member "P a[4]" and P has a nontrivial default constructor and
// destructor).  When the constructor of one element throws, the elements
// that were already constructed must be destroyed exactly once.  Before
// the fix, the generated per-element helper had no cleanup for earlier
// elements (or registered the cleanup of one element only), and cleanups
// of preceding array members were lost.

int ctors, dtors, throw_at, live, errors;

struct E {
  int v;
  E() : v(-1) { if (++ctors == throw_at) throw 1; ++live; }
  ~E() { ++dtors; --live; }
};
struct P { int i; E e; };
struct Q { E e1; int i; E e2; };
struct H { P a[4]; int tail = 9; };
struct G { P a[4]; };
struct GQ { Q a[3]; };
struct G2 { P a[2][3]; };
struct G3 { P a[2][2][2]; };
struct N { G g[2]; };
struct GV { int k = 5; Q a[2]; P b[2][2]; };
struct G1 { P a[3][1]; };
struct G12 { P a[3][1][2]; };

template <class F> static void check(int n, F f) {
  for (throw_at = 1; throw_at <= n; ++throw_at) {
    bool threw = false;
    ctors = dtors = live = 0;
    try { f(); } catch (int) { threw = true; }
    if (!threw || dtors != throw_at - 1 || live != 0) ++errors;
  }
}

int main() {
  check(4, [] { H x{}; });
  check(4, [] { G x{}; });
  check(6, [] { GQ x{}; });
  check(6, [] { G2 x{}; });
  check(8, [] { G3 x{}; });
  check(8, [] { N x{}; });
  check(8, [] { GV x{}; });
  check(12, [] { G2 x[2]{}; });
  check(3, [] { G1 x{}; });
  check(6, [] { G12 x{}; });
  check(3, [] { (void)G1{}; });
  check(4, [] { static G x{}; });
  check(4, [] { (void)G{}; });
  check(4, [] { G *p = new G{}; delete p; });
  /* Success path: everything constructed is destroyed. */
  throw_at = 0; live = 0;
  { G2 a{}; GV b{}; G c[2]{}; N d{}; G1 e{}; G12 f{}; }
  if (live != 0) ++errors;
  return errors != 0;
}
