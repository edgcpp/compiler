//type:cp
//options_all:--c++20 --microsoft_v 1930
namespace std {
    template <bool, class = void>
    struct enable_if {};
    template <class T>
    struct enable_if<true, T> { using type = T; };
    template <bool B, class T = void>
    using enable_if_t = typename enable_if<B, T>::type;

    template <class F, class T>
    inline constexpr bool is_convertible_v = __is_convertible_to(F, T);

    template <class T>
    struct basic_string_view { basic_string_view(const T *) {} };
    using string_view = basic_string_view<char>;
}

template <class T>
struct FormatArg { FormatArg(const std::basic_string_view<T> &) {} };
template <class T, class = std::enable_if_t<std::is_convertible_v<T, std::string_view>>>
FormatArg(T arg) -> FormatArg<char>;
template <class T>
using FA = FormatArg<T>;

void f() { FA fa("test"); }
