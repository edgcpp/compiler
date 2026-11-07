//type:rp
//options_all:--c++20 --exceptions
//remark:Temporaries in aggregate initializers when an exception is thrown
//options:--g++ --gnu_version=130300:--strict
// Aggregate members with nontrivial destructors, where a later member's
// initializer throws: every member constructed before the exception, and
// every temporary, is destroyed exactly once.
int live_temps, made, destroyed, fails;
struct Temp { Temp() { ++live_temps; } ~Temp() { --live_temps; } };
struct D {
  D(const Temp &) { ++made; }
  D(const D &) = delete;
  ~D() { ++destroyed; }
};
struct Agg { D a; D b; int c; };
struct Outer { Agg in; D d; };
int check(int expected) { fails += live_temps != expected; return 0; }
int thrower() { throw 1; }
void expect(int n) {
  fails += made != n || destroyed != n || live_temps != 0;
  made = destroyed = 0;
}
int main() {
  try { Agg x{D(Temp{}), D(Temp{}), thrower()}; (void)x; } catch (int) {}
  expect(2);
  try { Agg y{D(Temp{}), D((thrower(), Temp{})), 0}; (void)y; } catch (int) {}
  expect(1);
  try {
    Outer o{{D(Temp{}), D(Temp{}), check(2)}, D((thrower(), Temp{}))};
    (void)o;
  } catch (int) {}
  expect(2);
  {
    Agg z{D(Temp{}), D(Temp{}), check(2)};
    check(0);
    (void)z;
  }
  expect(2);
  // An exception after the aggregate is complete destroys each member once.
  try {
    Agg w{D(Temp{}), D(Temp{}), check(2)};
    (void)w;
    thrower();
  } catch (int) {}
  expect(2);
  try {
    Outer v{{D(Temp{}), D(Temp{}), check(2)}, D(Temp{})};
    (void)v;
    thrower();
  } catch (int) {}
  expect(3);
  return fails;
}
