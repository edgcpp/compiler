//options_all:--c++20
template <class T> requires requires(const T& x) { x == x; }
void f();

template <class>
struct _Ioterator {
    friend bool operator==(const _Ioterator&, const _Ioterator&) = default;
};

void g() {
    f<_Ioterator<int>>();
}
