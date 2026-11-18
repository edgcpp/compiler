//options_all:--gnu_version 60000
struct x {
    constexpr x(int val) {
        const auto mu = __builtin_expect(val, 0);
    }
};

constexpr x x1{ 3 };
