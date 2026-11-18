//options_all:--c++17
typedef decltype(sizeof(0)) size_t;
inline void *operator new(size_t, void *_Where) noexcept { return _Where; }

inline void operator delete(void *, void *)noexcept { return; }

template <class _Ty> struct remove_reference { using type = _Ty; };
template <class _Ty> struct remove_reference<_Ty &> { using type = _Ty; };
template <class _Ty> struct remove_reference<_Ty &&> { using type = _Ty; };
template <class _Ty> using remove_reference_t = typename remove_reference<_Ty>::type;

template <class _Ty> constexpr _Ty &&forward(remove_reference_t<_Ty> &_Arg) noexcept {
    return static_cast<_Ty &&>(_Arg);
}

template <class _Ty> constexpr _Ty &&forward(remove_reference_t<_Ty> &&_Arg) noexcept {
    return static_cast<_Ty &&>(_Arg);
}

template <class... _Types> using void_t = void;

// STRUCT TEMPLATE _Add_reference
template <class _Ty, class = void> struct _Add_reference {
    using _Lvalue = _Ty;
    using _Rvalue = _Ty;
};

template <class _Ty> struct _Add_reference<_Ty, void_t<_Ty &>> {
    using _Lvalue = _Ty &;
    using _Rvalue = _Ty &&;
};

template <class _Ty> struct add_rvalue_reference { using type = typename _Add_reference<_Ty>::_Rvalue; };
template <class _Ty> using add_rvalue_reference_t = typename _Add_reference<_Ty>::_Rvalue;

template <class _Ty> add_rvalue_reference_t<_Ty> declval() noexcept;

template <class _Ty, class... _Types>
auto construct_at(_Ty *const _Location, _Types &&... _Args)
    -> decltype(::new (const_cast<void *>(static_cast<const volatile void *>(_Location)))
                    _Ty(forward<_Types>(_Args)...)) {
    return ::new (const_cast<void *>(static_cast<const volatile void *>(_Location))) _Ty(forward<_Types>(_Args)...);
}

template <class _Void, class Ty, class... Types> inline constexpr bool can_construct_at = false;

template <class Ty, class... Types>
inline constexpr bool
    can_construct_at<void_t<decltype(construct_at(declval<Ty *>(), declval<Types>()...))>, Ty, Types...> = true;

struct X {};

static_assert(!can_construct_at<void, int, X>);
static_assert(!can_construct_at<void, X, int>);
