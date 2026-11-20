//options_all:--microsoft --c++14
void f()
{
  int (*p)(int) = [](int a) -> decltype(a){ return 1; };
}
