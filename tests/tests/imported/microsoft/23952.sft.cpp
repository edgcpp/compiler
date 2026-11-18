//options_all:--c++20
namespace std {
    template <class T, class U>
    constexpr bool is_same_v = false;

    template <class T>
    constexpr bool is_same_v<T, T> = true;

    template <class T, class U>
    concept same_as = is_same_v<T, U>;
}

template <class T>
concept string_c = requires (const T &str) {
    { str.c_str() } -> ::std::same_as<typename T::const_pointer>;
};
