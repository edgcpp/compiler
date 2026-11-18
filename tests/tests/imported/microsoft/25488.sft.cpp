//options_all:--ms_c++17 --no_ms_permissive
//options:--microsoft_version=1934;fp:--microsoft_version=1933;fn
template <class, class>
inline constexpr bool is_same_v = false;
template <class T>
inline constexpr bool is_same_v<T, T> = true;

template <class T>
inline constexpr bool is_enum_v = __is_enum(T);

template <class T, bool = is_enum_v<T>>
struct _Underlying_type { using type = __underlying_type(T); };
template <class T>
struct _Underlying_type<T, false> {};
template <class T>
using underlying_type_t = typename _Underlying_type<T>::type;

enum Unsigned {
    A = 0xFFFFFFFF // Value 'A' does not fit in 'int'.
};

// Previously would fail this static_assert. Now passes.
static_assert(is_same_v<underlying_type_t<Unsigned>, unsigned int>);

template <typename T>
void f(T x) {}

int main() {
    // Previously called f<int>, now calls f<unsigned int>.
    f(+A);
}
