//options_all:--microsoft --c++14
template <typename T1, typename T2>
void f(T1, T2) {}

template <void F(int, int)>
void g() {}

void h()
{
   g<f<int>>();
}
