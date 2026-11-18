//type:cp
//options_all:--c++14 --microsoft_v 1900
template <bool Val> struct bool_constant {
    static constexpr bool value = Val;
};
template <bool Cond, typename T = void> struct enable_if { };
template <typename T> struct enable_if<true, T> {
    typedef T type;
};
template <typename From, typename To> struct is_convertible
    : bool_constant<__is_convertible_to(From, To)> { };
template <typename T, typename... Args> struct is_constructible
   : bool_constant<__is_constructible(T, Args...)> { };
template <typename A, typename B> struct pair {
    template <typename X, typename Y,
        typename = typename enable_if<is_constructible<A, X>::value && is_constructible<B, Y>::value>::type,
        typename enable_if<is_convertible<X, A>::value && is_convertible<Y, B>::value, int>::type = 0
    > pair(const pair<X, Y>&) { }
};
struct Meow { };
class Kitty {
public:
    Kitty(Meow&&) { }
private:
    Kitty(const Kitty&) { }
    Kitty(Kitty&&) { }
};
void test(const pair<Meow, int>& src) {
    pair<Kitty, int> p(src);
}
