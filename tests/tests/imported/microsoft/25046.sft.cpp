//options_all:--c++20
using size_t = decltype(sizeof(0));

template <class T, size_t N>
struct array {
    constexpr const T *begin() const { return elems; }
    constexpr const T *end() const { return elems + N; }

    constexpr T &operator[](size_t i) { return elems[i]; }
    constexpr const T &operator[](size_t i) const { return elems[i]; }

    T elems[N];
};

template <class T, class ...Ts>
array(T, Ts ...) -> array<T, 1 + sizeof...(Ts)>;

template <size_t N>
constexpr auto transform(const array<float, N> &arr) {
    array<int, N> values;
    for (int i{ 0 }; const auto &k : arr) {
        values[i] = (int)k;
        ++i;
    }
    return values;
}

constexpr auto values = transform(array{ 0.f });
