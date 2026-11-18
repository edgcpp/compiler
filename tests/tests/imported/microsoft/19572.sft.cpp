//options_all:--microsoft_version 1914 --ms_c++14
template <typename... Types>
    using void_t = void;
template <typename T>
    constexpr bool is_default_constructible_v = __is_constructible(T);
template <typename T, typename = void>
    struct IsImplicitlyDefaultConstructible {
    static constexpr bool value = false;
};
template <typename T>
    void ImplicitlyDefaultConstruct(const T&);
template <typename T>
    struct IsImplicitlyDefaultConstructible<T,
        void_t<decltype(ImplicitlyDefaultConstruct<T>({}))>> {
    static constexpr bool value = true;
};
struct NoDefault {
    NoDefault(int, int) { }
};
struct ExplicitDefault {
    explicit ExplicitDefault() = default;
};
static_assert(is_default_constructible_v<int>, "is_default_constructible_v<int>");
static_assert(IsImplicitlyDefaultConstructible<int>::value, "IsImplicitlyDefaultConstructible<int>::value");
static_assert(!is_default_constructible_v<NoDefault>, "!is_default_constructible_v<NoDefault>");
static_assert(!IsImplicitlyDefaultConstructible<NoDefault>::value, "!IsImplicitlyDefaultConstructible<NoDefault>::value");
static_assert(is_default_constructible_v<ExplicitDefault>, "is_default_constructible_v<ExplicitDefault>");
static_assert(!IsImplicitlyDefaultConstructible<ExplicitDefault>::value, "!IsImplicitlyDefaultConstructible<ExplicitDefault>::value");
