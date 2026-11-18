//options_all:--microsoft --c++14
struct A
{
  A(A&&) {}
};
struct B
{
  A a;
  B(B&&) noexcept = default;
};
