//options_all:--c++14 --microsoft
struct Base {
    __declspec(nothrow) Base() { }
};
struct Derived : Base {
    Derived() noexcept = default;
};
