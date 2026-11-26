//type:cp
//options_all:--c++23
struct Base {
    template <class T>
    void f(this T &self) {}
};

struct Derived : Base {
    void g() {
        f();
    }
};
