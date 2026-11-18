//type:rp
//options_all:--c++14 --exceptions

// Throwing an aggregate whose initialization has cleanups for partially
// constructed subobjects (e.g., "throw H{E(1)}" with two class members).
// Once the exception object is completely initialized, those cleanups must
// no longer be active: before the fix, they were still active when the
// exception was thrown, so the subobjects were destroyed during unwinding
// and again when the exception object was destroyed.

int live, errors;

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
struct T { int v; T(int x) : v(x) {} ~T() {} };
static int tv(const T &t) { return t.v; }

struct H2 { E a, b; };
struct H4 { E a[2]; };
struct H5 { E a[2]; E b; };
struct H6 { E a[2][2]; };
struct L { E a; E b; int i; };
struct X { E a; Thrower t; };

template <class F> static void run(F f) {
  live = 0;
  try { f(); } catch (...) {}
  if (live != 0) ++errors;
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
  run([] { E x(1), y(2); throw [x, y] { return x.v + y.v; }; });
  // An enclosing local must still be destroyed (once) during unwinding.
  run([] { E outer(1); throw H2{E(7)}; });
  // A subobject initializer that throws: the subobjects constructed so far
  // must be destroyed (once), and no exception object is thrown.
  run([] { throw X{E(1)}; });
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
