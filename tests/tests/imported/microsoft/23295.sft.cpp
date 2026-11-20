//options_all:--c++17
namespace notstd {
    template <bool Val> struct bool_constant {
        static constexpr bool value = Val;
    };

    using true_type  = bool_constant<true>;
    using false_type = bool_constant<false>;

    template <bool, class T, class> struct conditional { using type = T; };
    template <class T, class F> struct conditional<false, T, F> {
        using type = F;
    };

    template <bool Test, class T, class F> using conditional_t =
        typename conditional<Test, T, F>::type;

    template <class, class> inline constexpr bool is_same_v = false;
    template <class T> inline constexpr bool is_same_v<T, T> = true;

    template <class T, class U> struct is_same
        : bool_constant<is_same_v<T, U>> {};

    template <bool FirstValue, class First, class... Rest>
    struct disjunction_impl {
        using type = First;
    };

    template <class First, class Next, class... Rest>
    struct disjunction_impl<false, First, Next, Rest...> {
        using type = typename disjunction_impl<Next::value, Next, Rest...>::type;
    };

    template <class...> struct disjunction : false_type {};

    template <class First, class... Rest> struct disjunction<First, Rest...>
        : disjunction_impl<First::value, First, Rest...>::type {};

    template <class... Traits> inline constexpr bool disjunction_v =
        disjunction<Traits...>::value;

    template <bool, class First, class...> struct conjunction_impl {
        using type = First;
    };

    template <class First, class Next, class... Rest>
    struct conjunction_impl<true, First, Next, Rest...> {
        using type = typename conjunction_impl<Next::value, Next, Rest...>::type;
    };

    template <class...> struct conjunction : true_type {};

    template <class First, class... Rest> struct conjunction<First, Rest...>
        : conjunction_impl<First::value, First, Rest...>::type {};

    template <class... Traits> inline constexpr bool conjunction_v =
        conjunction<Traits...>::value;

    template <class> inline constexpr bool is_void_v = false;
    template <> inline constexpr bool is_void_v<void> = true;

    template <class T> struct is_void : bool_constant<is_void_v<T>> {};

    template <class...> using void_t = void;

    template <class From, class To> struct is_convertible
        : bool_constant<__is_convertible_to(From, To)> {};

    template <class From, class To>
    inline constexpr bool is_convertible_v = __is_convertible_to(From, To);

    template <class T>
    T&& declval() noexcept;

    template <class To>
    void implicitly_convert_to(To) noexcept;

    template <class From, class To, bool = is_convertible_v<From, To>, bool = is_void_v<To>>
    inline constexpr bool is_nothrow_convertible_v =
        noexcept(implicitly_convert_to<To>(notstd::declval<From>()));

    template <class From, class To, bool Void>
    inline constexpr bool is_nothrow_convertible_v<From, To, false, Void> = false;

    template <class From, class To>
    inline constexpr bool is_nothrow_convertible_v<From, To, true, true> = true;

    template <class From, class To> struct is_nothrow_convertible
        : bool_constant<is_nothrow_convertible_v<From, To>> {};

    template <class, class> struct invoke_traits_zero;

    template <class Callable> using decltype_invoke_zero =
        decltype(notstd::declval<Callable>()());

    template <class Callable>
    struct invoke_traits_zero<void_t<decltype_invoke_zero<Callable>>, Callable> {
        using is_nothrow_invocable = bool_constant<noexcept(notstd::declval<Callable>()())>;
        template <class R>
        using is_nothrow_invocable_r = bool_constant<conjunction_v<is_nothrow_invocable,
            disjunction<is_void<R>, is_nothrow_convertible<decltype_invoke_zero<Callable>, R>>>>;
    };

    template <class Callable, class... Ts> using select_invoke_traits =
        conditional_t<sizeof...(Ts) == 0, invoke_traits_zero<void, Callable>, void>;

    template <class> struct invoker_ret {
        template <class F, class... Args>
        static void call(F&& f, Args&&... args) noexcept(
            select_invoke_traits<F, Args...>::template is_nothrow_invocable_r<int>::value) {}
    };
} // namespace notstd

int main() {
    notstd::invoker_ret<void>::call([] { return 42; });
}
