//options:--gnu_version 90000 --c++14:--clang_version 30500 --c++14:--clang_version 50000 --c++20
struct ctor { ctor(); };
struct D { ctor x; int y; };
D d() {
    return { .y = 0 };
}

struct F { int x = 0; int y = 0; };
F f() {
    return { .y = 0 };
}
