//options_all:--microsoft --c++14
struct A
{
  int x;
  constexpr A() : x(42) {}
};
constexpr A a;
constexpr int A::*pdm = &A::x;
constexpr int y = a.*pdm;
