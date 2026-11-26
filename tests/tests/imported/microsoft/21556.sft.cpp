//options_all:--c++17
template <class T> T&& declval() noexcept;

template <class T, T Value>
struct integral_constant {
    static constexpr T value = Value;
};

template <class>
struct is_nothrow_swappable : integral_constant<bool, true> {};

template <bool, class First, class... Rest>
struct conjunction_impl {
    using type = First;
};

template <class True, class Next, class... Rest>
struct conjunction_impl<true, True, Next, Rest...> {
    using type = typename conjunction_impl<Next::value, Next, Rest...>::type;
};

template <class...>
struct conjunction : integral_constant<bool, true> {};

template <class First, class... Rest>
struct conjunction<First, Rest...> :
    conjunction_impl<First::value, First, Rest...>::type {};

template <class... Traits>
constexpr bool conjunction_v = conjunction<Traits...>::value;

template <class F>
constexpr bool nothrow_visit = noexcept(declval<F>()(42));

template <class F>
constexpr auto visit(F&& f) noexcept(nothrow_visit<F>) ->
    decltype(declval<F>()(42))
{
    return static_cast<F&&>(f)(42);
}

template <class... Types>
struct variant {
    void swap(variant& that) {
        visit([](auto) {
            visit([](auto) noexcept(
                conjunction_v<is_nothrow_swappable<Types>...>){});
        });
    }
};

struct nontrivial {
    ~nontrivial() {}
};

int main() {
    variant<nontrivial> v0, v1;
    v0.swap(v1);
}
