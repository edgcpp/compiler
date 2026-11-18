//type:fn
//options_all:--clang
namespace A {
    template <class T>
    struct E;
}

template <class T>
struct E {};

using namespace A;

void f() {
    E<int> e;
}
