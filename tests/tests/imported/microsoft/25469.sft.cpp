//options_all:--c++20
template <class>
concept C = ([]{}, true);

void f(C auto) {}

template <C>
struct S {};

int main() {
    f(42);
    S<int> s;
    static_assert(C<int>);
}
