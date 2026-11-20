//options_all:--microsoft_v 1913 --ms_c++latest
template <typename T, typename U> struct is_same {
    static constexpr bool value = false;
};
template <typename T> struct is_same<T, T> {
    static constexpr bool value = true;
};

template <typename T>
struct S {};

template <typename T>
char Func(const S<T>&) { return 'x'; }

template <typename T, typename U = int>
double Func(const T&, U = 0) { return 3.14; }

static_assert(is_same<decltype(Func(S<int>{})), char>::value, "BOOM");
