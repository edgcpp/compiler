//options_all:--c++20
template <class F, class ...As>
struct bind_back_wrap {
    constexpr bind_back_wrap(const F &, As &&...) {}

    constexpr void operator()(auto &&...args) const {}
};

auto add = [](const auto &a, const auto &b) { return a + b; };

void f() {
    auto foo = bind_back_wrap(add, 2);
    foo(1);
}
