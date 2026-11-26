//options_all:--microsoft --c++14
struct A
{
  int x;
  constexpr A() : x(42) {}
};
constexpr A a;
constexpr int A::* pdm = &A::x;
constexpr const A* const pa = &a;
static_assert(a.*pdm == 42, "1");
static_assert(pa->*pdm == 42, "2");
