//options_all:--ms_c++latest --microsoft_v 1914
template <class... _Types> using void_t = void;
template <class _Iter, class = void>
inline constexpr bool _Range_verifiable_v = false;
template <class _Iter>
inline constexpr bool
    _Range_verifiable_v<_Iter, void_t<decltype(_Verify_range(_Iter{}))>> = true;
template <class _Iter> constexpr void _Adl_verify_range(const _Iter &_First) {
    // real code does things other than static_assert
    static_assert(_Range_verifiable_v<_Iter>, "BOOM");
}
template <class _Ty> struct _Array_const_iterator {
    friend constexpr void _Verify_range(const _Array_const_iterator &_First) {}
};
template <class _Ty>
struct _Array_iterator : _Array_const_iterator<_Ty> {};
