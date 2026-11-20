//type:rp
//options_all:--c++14 --exceptions

// Throwing an aggregate whose initialization has cleanups for partially
// constructed subobjects (e.g., "throw H{E(1)}" with two class members).
// Once the exception object is completely initialized, those cleanups must
// no longer be active: before the fix, they were still active when the
// exception was thrown, so the subobjects were destroyed during unwinding
// and again when the exception object was destroyed.  Temporaries with
// destructors in the initializer must still be destroyed (once).

int live, tlive, errors;

struct E {
  int v;
  E(int x = -1) : v(x) { ++live; }
  E(const E &o) : v(o.v) { ++live; }
  ~E() { if (--live < 0) ++errors; }
};
struct Thrower {
  Thrower() { throw 0; }
  ~Thrower() { ++errors; }
};
struct T {
  int v;
  T(int x) : v(x) { ++tlive; }
  T(const T &o) : v(o.v) { ++tlive; }
  ~T() { if (--tlive < 0) ++errors; }
};
static int tv(const T &t) { return t.v; }
static E te(const T &t) { return E(t.v); }

struct H2 { E a, b; };
struct H4 { E a[2]; };
struct H5 { E a[2]; E b; };
struct H6 { E a[2][2]; };
struct L { E a; E b; int i; };
struct L2 { int i; E a; E b; };
struct L3 { E a; int i; E b; };
struct M { L l; E c; };
struct X { E a; Thrower t; };
struct X2 { E a; int i; Thrower t; };

template <class F> static void run(F f) {
  live = 0;
  tlive = 0;
  try { f(); } catch (...) {}
  if (live != 0 || tlive != 0) ++errors;
}

int main() {
  run([] { throw H2{E(7)}; });
  run([] { throw H2{E(7), E(8)}; });
  run([] { throw H4{{E(7)}}; });
  run([] { throw H5{{E(7)}}; });
  run([] { throw H5{{E(1), E(2)}, E(3)}; });
  run([] { throw H6{{{E(7)}}}; });
  run([] { throw L{E(1), E(2), tv(T(3))}; });
  run([] { throw H2{E(tv(T(7)))}; });
  run([] { throw L2{tv(T(3)), E(1), E(2)}; });
  run([] { throw L3{E(1), tv(T(3)), E(2)}; });
  run([] { throw H2{te(T(7)), te(T(8))}; });
  run([] { throw H2{E(1), te(T(8))}; });
  run([] { throw H5{{E(1), te(T(2))}, te(T(3))}; });
  run([] { throw M{{E(1), E(2), tv(T(3))}, te(T(4))}; });
  run([] { throw M{{E(1), te(T(2)), tv(T(3))}, E(4)}; });
  run([] { E x(1), y(2); throw [x, y] { return x.v + y.v; }; });
  run([] { E x(1); throw [x, y = te(T(2))] { return x.v + y.v; }; });
  // An enclosing local must still be destroyed (once) during unwinding.
  run([] { E outer(1); throw H2{E(7)}; });
  run([] { E outer(1); throw L3{E(1), tv(T(3)), te(T(4))}; });
  // A subobject initializer that throws: the subobjects constructed so far
  // must be destroyed (once), and no exception object is thrown.
  run([] { throw X{E(1)}; });
  run([] { throw X2{E(1), tv(T(3))}; });
  // The cleanup state after a caught exception must be intact.
  run([] {
    E outer(1);
    try { throw H4{{E(2)}}; } catch (const H4 &h) {
      if (h.a[0].v != 2 || h.a[1].v != -1) ++errors;
    }
    throw H2{E(3)};
  });
  return errors != 0;
}
