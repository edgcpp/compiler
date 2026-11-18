//options_all:--microsoft --time_limit 5
template <class T> void f(T*) {}
template <class T> decltype(f(f(T()))) f(T&) {}

void g(int* p)
{
    f(p);
}
