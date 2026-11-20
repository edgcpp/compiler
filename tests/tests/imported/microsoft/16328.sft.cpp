//options_all:--microsoft --c++14
constexpr int zero()
{
  return 0;
}
constexpr int (&rzero)() = zero;
constexpr int x = rzero();
