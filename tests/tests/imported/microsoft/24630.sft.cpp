//options_all:--c++20
template <unsigned N>
struct vec {
    constexpr static unsigned Count = N;
    void z() requires (Count >= 3) {}
};

void f(vec<3> v) { v.z(); }
