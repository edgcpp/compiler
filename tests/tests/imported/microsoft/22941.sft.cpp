//options_all:--c++20
//type:fp
template <class _Ty>
concept destructible = __is_nothrow_destructible(_Ty);

template <class _Se, bool _Store = !destructible<_Se>>
class _Subrange_base {};

template <class _Se>
class subrange : public _Subrange_base<_Se> {};
