//options_all:--c++17
struct A {};
struct B {};

struct Test {
    template <class T>
    struct overloaded : T {};

    template <class T>
    overloaded(T) -> overloaded<T>;

    overloaded(A) -> overloaded<A>;

    void f() {
        overloaded obj1{ A{} };
        overloaded obj2{ B{} };
    }
};

void f() {
    Test::overloaded obj1{ A{} };
    Test::overloaded obj2{ B{} };
}
