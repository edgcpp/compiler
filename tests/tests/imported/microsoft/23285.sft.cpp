//options_all:--c++17
void f(int) noexcept {}

template <class T>
struct S {
    T x;

    template <class = void>
    void g() noexcept(noexcept(f(x))) {}
};

int main() {
    S<int> s;
    static_assert(noexcept(s.g()), "FAIL");
}
