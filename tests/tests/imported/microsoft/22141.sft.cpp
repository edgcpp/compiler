//options_all:--microsoft_v 1925 --c++17
template <class T, size_t N>
struct array {};

template <class T>
struct remove_cv { using type = T; };
template <class T>
struct remove_cv<const T> { using type = T; };

template <typename T, size_t Extent>
struct span {
    using value_type = typename remove_cv<T>::type;

    template <size_t N>
    constexpr span(const array<value_type, N> &arr) {}
};

template <class T, size_t N>
span(const array<T, N> &) -> span<const T, N>;

void foo(const array<int, 3> values) {
    const span value_span{ values };
}
