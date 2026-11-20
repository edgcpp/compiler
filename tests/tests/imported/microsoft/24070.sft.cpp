//options_all:--microsoft --microsoft_bugs --microsoft_version 1929 --ms_c++latest --no_ms_permissive --no_warnings
template <bool, class = void>
struct enable_if {};
template <class T>
struct enable_if<true, T> { using type = T; };
template <bool Test, class T = void>
using enable_if_t = typename enable_if<Test, T>::type;

template <class ...>
using void_t = void;

template <class, class = void>
struct add_reference;
template <class T>
struct add_reference<T, void_t<T &>> { using type = T &&; };
template <class T>
using add_rvalue_reference_t = typename add_reference<T>::type;

template <class T>
add_rvalue_reference_t<T> declval();

template <class T, class ...Ts>
inline constexpr bool is_constructible_v = __is_constructible(T, Ts...);

template <class To, class From>
inline constexpr bool is_assignable_v = __is_assignable(To, From);

template <class T>
auto construct_array(T (&&)[1]) -> T;

template <class T>
struct variant_init_overload {
    template <class U>
    auto operator()(T, U &&) -> decltype(construct_array<T>({ declval<U>() }));
};
template <class ...Ts>
struct variant_init_overload_set : variant_init_overload<Ts>... {
    using variant_init_overload<Ts>::operator()...;
};

template <class, class, class ...>
struct variant_init_helper;
template <class T, class ...Ts>
struct variant_init_helper<
    void_t<decltype(variant_init_overload_set<Ts...>{}(declval<T>(), declval<T>()))>,
    T, Ts...
> {
    using type = decltype(variant_init_overload_set<Ts...>{}(declval<T>(), declval<T>()));
};

template <class T, class ...Ts>
using variant_init_type = typename variant_init_helper<void, T, Ts...>::type;

template <class ...Ts>
struct variant {
    template <class T, enable_if_t<is_constructible_v<variant_init_type<T, Ts...>, T>, int> = 0>
    constexpr variant(T &&);

    variant &operator=(const variant &) = delete;
    variant &operator=(variant &&) = delete;
};

static_assert(!is_assignable_v<variant<int, const bool>, decltype("meow")>, "");
static_assert(!is_assignable_v<variant<int, const volatile bool>, decltype("meow")>, "");

struct string { string(const char *); };
void f() { variant<string, bool> v = "bar"; }

void Overload(int) {}
void Overload(variant<unsigned short>) {}
void g() { Overload(42); }
