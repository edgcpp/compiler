//type:fp
//options_all:--c++17
template<bool Value> struct bool_constant {
    static constexpr auto value = Value;
};

template<bool, class> struct enable_if {};
template<class T> struct enable_if<true, T> {
    using type = T;
};
template<bool B, class T> using enable_if_t =
    typename enable_if<B, T>::type;

#ifdef WORKAROUND1
template<class U> void f(U b) {
    [b](auto) { // MSVC needs the capture (known bug)
        if constexpr (decltype(b)::value) {
            using T = enable_if_t<decltype(b)::value, int>;
            T x = 42;
        }
    }(42);
}
#endif

int main() {
#ifdef WORKAROUND1
    f(bool_constant<false>{});
#else // WORKAROUND1
    [](auto b) {
#ifdef WORKAROUND2
        [b](int) {
#else
        [b](auto) {
#endif
            if constexpr (decltype(b)::value) {
                using T = enable_if_t<decltype(b)::value, int>;
                T x = 42;
            }
        }(42);
    }(bool_constant<false>{});
#endif // WORKAROUND1
}
