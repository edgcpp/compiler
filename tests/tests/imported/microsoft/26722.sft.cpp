//type:cp
//options_all:--c++17
constexpr bool f() {
    int x[10]{};
    return x + 10 != nullptr;
}

int main() {
    static_assert(f());
}
