//type:cp
//options_all:--c++20
template <bool, class = void>
struct enable_if {};
template <class T>
struct enable_if<true, T> { using type = T; };
template <bool B, class T = void>
using enable_if_t = typename enable_if<B, T>::type;

template <class _Ty>
inline constexpr bool is_enum_v = __is_enum(_Ty);

template <class T>
using underlying_type_t = __underlying_type(T);

template <class E>
constexpr enable_if_t<is_enum_v<E>, underlying_type_t<E>> int_enum(const E value) {
    return static_cast<underlying_type_t<E>>(value);
}

enum class Studio { week, month };

template <const auto n = int_enum(Studio::month)>
struct DBTableBase {};

struct NoticeInfo : DBTableBase<> {};
