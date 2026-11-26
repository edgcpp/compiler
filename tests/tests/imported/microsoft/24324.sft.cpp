//options_all:--c++20
template <class T>
struct A {
    T value;

    constexpr A() noexcept = default;
    constexpr ~A()         = default;
};

template <class T>
struct nontrivial_A {
    T value;

    constexpr nontrivial_A(T in = T{}) noexcept : value(in) {}
    constexpr ~nontrivial_A() {}
};

constexpr bool test_compiletime_destroy_variants() {
    {
        A<int>* a = new A<int>[10];
        a <= a + 10;
        delete[] a;
    }
    {
        nontrivial_A<int>* a = new nontrivial_A<int>[10];
        a <= a + 10;
        delete[] a;
    }
    return true;
}
static_assert(test_compiletime_destroy_variants());
