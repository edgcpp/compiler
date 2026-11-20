//type:fp
//options_all:--c++17 --microsoft
struct X2 {
    template <typename U>
    static constexpr U Value = sizeof(U);
};

static_assert(X2::Value<short> == 2, "BOOM!");

template <typename T>
constexpr T X2::Value;

static_assert(X2::Value<short> == 2, "BOOM!");
