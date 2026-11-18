//options_all:--ms_c++17
struct tuple { int x, y, z; };

void f() {
    auto [x, y, z] = tuple{ 1, 2, 3 };
    [x, y, z]() {};
}
