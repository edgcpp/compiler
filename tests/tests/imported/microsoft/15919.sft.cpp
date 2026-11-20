//options_all:--microsoft --c++14

struct A {
  int x;
  constexpr A() : x(0) {}
  constexpr A(int i) : x(i) {}
};
struct B {
  A a[2];
  constexpr B(int i) : a{i}{}
};
void f()
{
  B(37);
}
