//options_all:--c++17
template <bool _Test, class _Ty = void>
struct enable_if {};

template <class _Ty>
struct enable_if<true, _Ty> {
    using type = _Ty;
};

template <class... _Types>
using void_t = void;

template <class>
inline constexpr bool is_lvalue_reference_v = false;

template <class _Ty>
inline constexpr bool is_lvalue_reference_v<_Ty&> = true;

template <class _Ty1, class _Ty2, class = void>
struct _Common_reference2A {};

template <
    class _Ty1,
    class _Ty2,
    class _Result = _Ty1&,
    typename enable_if<is_lvalue_reference_v<_Result>, int>::type = 0
>
using _LL_common_ref = _Result;

template <class _Ty1, class _Ty2>
struct _Common_reference2A<_Ty1&, _Ty2&, void_t<_LL_common_ref<_Ty1, _Ty2>>> {
    using type = int;
};

struct simple_base {};
struct simple_derived : simple_base {};

_Common_reference2A<simple_base&, simple_derived&>::type x;
