//options_all:--microsoft_version 1913
template <typename T, typename = void>
struct A
{};

template <template <typename...> class T, typename U>
void f(T<U>)
{}

void g()
{
    A<int> a;
    f(a);
}
