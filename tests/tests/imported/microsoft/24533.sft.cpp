//options_all:--microsoft_version 1929 --ms_c++17
template <class T, class ...As>
inline constexpr bool is_constructible_v = __is_constructible(T, As...);

template <class T>
struct ExplicitTo {
    explicit operator T() const;
};

static_assert(is_constructible_v<int const &, ExplicitTo<int &&>>);
