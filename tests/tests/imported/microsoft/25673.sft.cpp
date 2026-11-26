//options_all:--c++14
template <class T>
struct S;

template <class T>
void f() {
    [data = T()](S<T> *p) {
        auto iter = p->f;
        [&]() { iter->second(data); };
    };
}
