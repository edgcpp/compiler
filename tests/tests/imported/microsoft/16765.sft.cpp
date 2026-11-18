//type:fn
//options_all:--microsoft --c++14
template <typename T>
void f(decltype(T::g<T>())) {}

struct A
{
  template <typename T>
  static int g();
};

void h()
{
  f<A>(0);
}
