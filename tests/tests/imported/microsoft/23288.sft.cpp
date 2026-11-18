//options_all:--c++20
//type:fp
template <class, class>
inline constexpr bool is_same_v = false;
template <class _Ty>
inline constexpr bool is_same_v<_Ty, _Ty> = true;

template <class _Ty>
concept integral = is_same_v<_Ty, int>;

template <class>
struct incrementable_traits {};

template <class _Ty>
struct incrementable_traits<const _Ty> : incrementable_traits<_Ty> {};

template <class _Ty>
concept _Can_difference = requires(const _Ty& __a, const _Ty& __b) {
    { __a - __b } -> integral;
};

template <class _Ty> requires _Can_difference<_Ty>
struct incrementable_traits<_Ty> {};

incrementable_traits<int *const> x;
