//options_all:--c++20
//type:fp
template <class T, unsigned long N>
struct array {
    constexpr const T *begin() const { return data; }
    constexpr const T *end() const { return data + N; }
    constexpr T *begin() { return data; }
    T data[N];
};

template <class T, unsigned long N>
constexpr array<T, N> reverse(const array<T, N> &bytes) {
    array<T, N> newBytes;
    auto _First = bytes.begin();
    auto _Last = bytes.end();
    auto _Dest = newBytes.begin();
    for (; _First != _Last; ++_Dest) {
        *_Dest = *--_Last;
    }
    return newBytes;
}

constexpr array<unsigned char, 8> x{ 1, 2, 3, 4, 5, 6, 7, 8 };
constexpr auto y = reverse(x);
