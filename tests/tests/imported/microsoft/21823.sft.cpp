//type:fn
//options_all:--microsoft_v 1912 --ms_c++17
struct A {
              A &operator=(const A &other) { // note: no exception specification; this function may throw.
                             return *this;
              }
};

struct B : public A {
              __declspec(nothrow) B &operator=(const B &other) = default;
};

void f(B b1, B b2) {
              b2 = b1; // error: attempting to reference a deleted function.
}
