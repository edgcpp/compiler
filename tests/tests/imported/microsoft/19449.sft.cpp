//options_all:--c++17
void f()
{
  [](auto (*pf)(int) -> int) {};
}
