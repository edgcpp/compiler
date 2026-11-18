//options_all:--c++20
//type:fp
template <bool _Test, class _Ty1, class _Ty2>
struct conditional { using type = _Ty1; };
template <class _Ty1, class _Ty2>
struct conditional<false, _Ty1, _Ty2> { using type = _Ty2; };
template <bool _Test, class _Ty1, class _Ty2>
using conditional_t = typename conditional<_Test, _Ty1, _Ty2>::type;

template <class, class>
inline constexpr bool is_same_v = false;
template <class _Ty>
inline constexpr bool is_same_v<_Ty, _Ty> = true;

template <class _Ty1, class _Ty2>
concept same_as = is_same_v<_Ty1, _Ty2>;

template <class _Ty>
concept _Is_from_primary = same_as<typename _Ty::_From_primary, _Ty>;

template <class>
struct iterator_traits;

template <class _Ty>
using iter_difference_t = typename conditional_t<
    _Is_from_primary<iterator_traits<_Ty>>,
    void,
    iterator_traits<_Ty>
> ::difference_type;

template <class _Ty>
struct iterator_traits<_Ty*> { using difference_type = long long; };

template <class _InIt>
iter_difference_t<_InIt> distance(_InIt) { return {}; }

void f(int* first) {
    distance(first);
}
