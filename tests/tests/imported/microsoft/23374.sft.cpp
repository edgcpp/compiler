//options_all:--c++20
template <class _Ty>
constexpr _Ty &&forward(_Ty &&_Arg) noexcept {
    return static_cast<_Ty &&>(_Arg);
}

template <class _Left, class _Right>
concept _Can_pipe = requires(_Left&& __l, _Right&& __r) {
    static_cast<_Right&&>(__r)(static_cast<_Left&&>(__l));
};

template <class _Derived>
struct _Base {
    template <_Can_pipe<const _Derived&> _Left>
    friend constexpr auto operator|(_Left&& __l, const _Base& __r)
        noexcept(noexcept(static_cast<const _Derived&>(__r)(forward<_Left>(__l))))
    {
        return static_cast<const _Derived&>(__r)(forward<_Left>(__l));
    }

    template <_Can_pipe<_Derived> _Left>
    friend constexpr auto operator|(_Left&& __l, _Base&& __r)
        noexcept(noexcept(static_cast<_Derived&&>(__r)(forward<_Left>(__l))))
    {
        return static_cast<_Derived&&>(__r)(forward<_Left>(__l));
    }
};

class _All_fn : public _Base<_All_fn> {};
