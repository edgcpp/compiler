//options_all:--microsoft_v 1929 --ms_c++latest
template <class _Ty1, class _Ty2>
inline constexpr bool
    is_layout_compatible_v = __is_layout_compatible(_Ty1, _Ty2);

template <class _Base, class _Derived>
inline constexpr bool is_pointer_interconvertible_base_of_v =
    __is_pointer_interconvertible_base_of(_Base, _Derived);

template <class _ClassTy, class _MemberTy>
constexpr bool
is_pointer_interconvertible_with_class(_MemberTy _ClassTy::*_Pm) noexcept {
  return __is_pointer_interconvertible_with_class(_ClassTy, _Pm);
}

template <class _ClassTy1, class _ClassTy2, class _MemberTy1, class _MemberTy2>
constexpr bool is_corresponding_member(_MemberTy1 _ClassTy1::*_Pm1,
                                       _MemberTy2 _ClassTy2::*_Pm2) noexcept {
  return __is_corresponding_member(_ClassTy1, _ClassTy2, _Pm1, _Pm2);
}
