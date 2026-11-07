//type:rp
//options_all:--c++20
//options:--g++ --gnu_version=130300:--strict
//remark:Temporaries in aggregate initializers live until the end of the init.

// A temporary created for an element of a braced aggregate initializer is
// destroyed when the initialization of the whole aggregate is complete, not
// at the end of the element.

int live, fails;
struct Temp { Temp() { ++live; } ~Temp() { --live; } };
struct A { A(const Temp&) {} };
struct Pair { A a; int b; };
int check(int expected) { fails += live != expected; return 0; }

Pair make_pair() { return {A(Temp{}), check(1)}; }

// Namespace-scope variable (dynamically initialized before main).
Pair ns{A(Temp{}), check(1)};

// Mem-initializer and default member initializer.
struct Holder {
  Pair p;
  Holder() : p{A(Temp{}), check(1)} {}
};
struct Nsdmi {
  Pair p{A(Temp{}), check(1)};
};

// A temporary bound to a reference member is lifetime-extended.
struct Ref { const Temp &r; int b; };

int main() {
  check(0);
  Pair p{A(Temp{}), check(1)};
  check(0);
  static Pair s{A(Temp{}), check(1)};
  check(0);
  Pair q = make_pair();
  check(0);
  Pair a[2]{{A(Temp{}), check(1)}, {A(Temp{}), check(2)}};
  check(0);
  struct Nested { Pair p; int c; } n{{A(Temp{}), check(1)}, check(1)};
  check(0);
  Holder h;
  check(0);
  Nsdmi d;
  check(0);
  Pair *np = new Pair{A(Temp{}), check(1)};
  check(0);
  delete np;
  {
    Ref r{Temp{}, check(1)};
    check(1);
    (void)r;
  }
  check(0);
  (void)p; (void)s; (void)q; (void)a; (void)n; (void)h; (void)d;
  return fails;
}
