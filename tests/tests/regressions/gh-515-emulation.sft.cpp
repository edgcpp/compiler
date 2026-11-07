//type:rp
//options_all:--c++20
//options:--g++ --gnu_version=110400:--ms_c++20 --microsoft_version=1944
//remark:Temporaries in aggregate initializers when emulating GCC 11 and MSVC

// GCC before 12 and MSVC destroy a temporary at the end of each element in
// the initializer of a variable, but keep it until the end of the complete
// initialization in other contexts.

int live, fails;
struct Temp { Temp() { ++live; } ~Temp() { --live; } };
struct A { A(const Temp&) {} };
struct Pair { A a; int b; };
int check(int expected) { fails += live != expected; return 0; }

Pair make_pair() { return {A(Temp{}), check(1)}; }
Pair ns{A(Temp{}), check(0)};
struct Holder {
  Pair p;
  Holder() : p{A(Temp{}), check(1)} {}
};
struct Nsdmi {
  Pair p{A(Temp{}), check(1)};
};

int main() {
  Pair p{A(Temp{}), check(0)};
  static Pair s{A(Temp{}), check(0)};
  Pair q = make_pair();
  Holder h;
  Nsdmi d;
  Pair *np = new Pair{A(Temp{}), check(1)};
  delete np;
  check(0);
  (void)p; (void)s; (void)q; (void)h; (void)d;
  return fails;
}
