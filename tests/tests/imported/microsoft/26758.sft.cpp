//type:cp
//options_all:--microsoft_v 1940 --ms_c++latest
// Repros with prod/fe x86chk as of 2023-10-09.
// Affects STL test P0896R4_views_iota.

// EDG ICE when a range-based `for` loop over lvalue `iota_view` is evaluated in a constant expression.

struct Ioterator {
    int Current;

    constexpr int operator*() const {
        return 0;
    }

    constexpr Ioterator& operator++() {
        ++Current;
        return *this;
    }

    friend constexpr bool operator==(const Ioterator& Left, const Ioterator& Right) {
        return Left.Current == Right.Current;
    }
};

struct fake_iota_view {
    using It = Ioterator;

    constexpr It begin() const {
        return It{ 0 };
    }
    constexpr It end() const {
        return It{ 1 };
    }
};

constexpr bool f() {
    auto rng = fake_iota_view{};
    for (const auto& e : rng) {
        (void)e;
    }
    return true;
}

static_assert(f());
