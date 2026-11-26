//type:cp
//options_all:--microsoft_v 1940 --ms_c++20
template<class, class>

constexpr bool my_is_same_v = false;
template<class T>
constexpr bool my_is_same_v<T, T> = true;

template<class T>
struct my_traits {};

template<class T>
requires (sizeof(T) >= sizeof(int))
struct my_traits<T> {
    template<class U>
    static constexpr bool value = my_is_same_v<T, U>;
};

template<class T>
struct Foo {
    using traits_type = my_traits<T>;

    template<class U>
        requires (traits_type::template value<U>)
    static void baz() {}
};

Foo<char> x [[maybe_unused]] {};
Foo<int> y [[maybe_unused]] {};

int main() {}
