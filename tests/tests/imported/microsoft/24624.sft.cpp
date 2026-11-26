//options_all:--c++20
template <class = int>
struct NTTP {};

template <class T, NTTP nttp>
struct A {
    static constexpr bool fn() { return true; }
};

template <NTTP nttp>
struct S {
    template <class T = int> requires (A<T, nttp>::fn())
    static constexpr bool test() { return true; }
};

void f() {
    S<NTTP<>{}>::test();
}
