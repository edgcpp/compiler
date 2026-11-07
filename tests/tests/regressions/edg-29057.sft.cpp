//type:rp
//options_all:--c++20
//options:--g++ --gnu_version=130300:--strict
int live, fails;
struct Temp { Temp() { ++live; } ~Temp() { --live; } };
struct A { A(const Temp&) {} };
struct Pair { A a; int b; };
int check(int expected) { fails += live != expected; return 0; }
Pair make_pair() { return {A(Temp{}), check(1)}; }
int main() {
  Pair p{A(Temp{}), check(1)};
  check(0);
  Pair q = make_pair();
  check(0);
  Pair a[2]{{A(Temp{}), check(1)}, {A(Temp{}), check(2)}};
  check(0);
  struct Nested { Pair p; int c; } n{{A(Temp{}), check(1)}, check(1)};
  check(0);
  (void)p; (void)q; (void)a; (void)n;
  return fails;
}
