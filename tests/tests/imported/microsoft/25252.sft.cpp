//options_all:--c++17
template <class T>
struct shared_ptr {};

template <typename T1, typename ...Ts>
struct Foo {
    Foo(const shared_ptr<T1> &t, const shared_ptr<Ts> &...ts) {}
};

void f(shared_ptr<int> ptr1, shared_ptr<double> ptr2) {
    Foo f(ptr1, ptr2);
}
