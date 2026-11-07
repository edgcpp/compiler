//type:rp
//options_all:--c++20
//options:--g++ --gnu_version=130300:--strict
//remark:Temporaries in a suspended and resumed aggregate initializer

int live, fails, idx;
struct Temp { Temp() { ++live; } ~Temp() { --live; } };
struct A { A(const Temp&) {} };
struct Pair { A a; int b; };
int check() { fails += live != ++idx; return 0; }
#define E1 {A(Temp{}), check()}
#define E10 E1, E1, E1, E1, E1, E1, E1, E1, E1, E1
#define E100 E10, E10, E10, E10, E10, E10, E10, E10, E10, E10
int main() {
  // More than 1,000 elements, so the parsing of the initializer is suspended
  // and resumed.
  Pair a[] = {E100, E100, E100, E100, E100, E100, E100, E100, E100, E100,
              E100};
  fails += live != 0 || idx != 1100;
  (void)a;
  return fails;
}
