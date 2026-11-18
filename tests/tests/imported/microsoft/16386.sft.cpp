//options_all:--microsoft
void f(int);

struct A
{
 template <typename T>
 auto f(T t) -> decltype(::f(t)) {}
};

void g() {
 A a;
 a.f(1);
}
