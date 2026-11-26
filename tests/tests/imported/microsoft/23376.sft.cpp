//options_all:--c++20
namespace std {

template <class, class>
constexpr bool is_same_v = false;
template <class T>
constexpr bool is_same_v<T, T> = true;

template <bool, class _Ty1, class>
struct conditional { using type = _Ty1; };
template <class _Ty1, class _Ty2>
struct conditional<false, _Ty1, _Ty2> { using type = _Ty2; };
template <bool _Test, class _Ty1, class _Ty2>
using conditional_t = typename conditional<_Test, _Ty1, _Ty2>::type;

template <class _Ty>
concept _Is_from_primary = is_same_v<typename _Ty::_From_primary, _Ty>;

template <class _Ty>
struct indirectly_readable_traits { using value_type = _Ty; };

template <class _It>
struct iterator_traits { using _From_primary = iterator_traits; };
template <class _Ty>
struct iterator_traits<_Ty*> { using value_type = _Ty; };

template <class _Ty>
using iter_value_t = typename conditional_t<
    _Is_from_primary<iterator_traits<_Ty>>,
    indirectly_readable_traits<_Ty>,
    iterator_traits<_Ty>
>::value_type;

}

template <class _RanItPat, class = std::iter_value_t<_RanItPat>>
struct boyer_moore_searcher { boyer_moore_searcher(const _RanItPat _First); };

void test_searchers(char *first) {
    boyer_moore_searcher bms1(first);
}
