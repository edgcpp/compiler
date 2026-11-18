//options_all:--c++20
template <class _Ty>
_Ty &&declval() noexcept;

template <class _Ty1, class _Ty2 = _Ty1>
using compare_three_way_result_t = decltype(declval<const _Ty1&>() <=> declval<const _Ty2&>());

template <class _Ty1, class _Ty2 = _Ty1>
struct compare_three_way_result {};

template <class _Ty1, class _Ty2>
     requires requires { typename compare_three_way_result_t<_Ty1, _Ty2>; }
struct compare_three_way_result<_Ty1, _Ty2> {
     using type = compare_three_way_result_t<_Ty1, _Ty2>;
};
