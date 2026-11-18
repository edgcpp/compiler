//options_all:--microsoft --c++14
void f(int k = [=]() noexcept { return 42; }()) noexcept
{}
static_assert(noexcept(f()), "f is NOT noexcept");
