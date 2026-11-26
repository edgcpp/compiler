//options_all:--microsoft --c++14
void sink(...){}

template<typename... T>
auto f(T... x)
{
[=](auto)
{
  sink(x...);
};
}

void g()
{
f();
}
