//options_all:--c++17
template <class T>
inline constexpr bool is_nothrow_destructible_v = __is_nothrow_destructible(T);

struct TrivialThrowingDtor {
    ~TrivialThrowingDtor() noexcept(false) = default;
};

static_assert(!is_nothrow_destructible_v<TrivialThrowingDtor>);
