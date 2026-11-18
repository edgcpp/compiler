//options_all:--microsoft_version 1920 --ms_c++17
template <class _Ty, _Ty _Val> struct integral_constant {
    static constexpr _Ty value = _Val;
    using value_type = _Ty;
    using type = integral_constant;
};
template <bool _Val> using bool_constant = integral_constant<bool, _Val>;
using true_type = bool_constant<true>;
using false_type = bool_constant<false>;
template <bool _Test, class _Ty = void> struct enable_if {};
template <class _Ty> struct enable_if<true, _Ty> { using type = _Ty; };
template <bool _Test, class _Ty = void> using enable_if_t = typename enable_if<_Test, _Ty>::type;
template <class _Ty1, class _Ty2> struct is_same : false_type {};
template <class _Ty1> struct is_same<_Ty1, _Ty1> : true_type {};
template <bool _First_value, class _First, class... _Rest> struct _Conjunction { using type = _First; };
template <class _True, class _Next, class... _Rest> struct _Conjunction<true, _True, _Next, _Rest...> {
    using type = typename _Conjunction<_Next::value, _Next, _Rest...>::type;
};
template <class... _Traits> struct conjunction : true_type {};
template <class _First, class... _Rest>
struct conjunction<_First, _Rest...> : _Conjunction<_First::value, _First, _Rest...>::type {};
template <class... _Traits> inline constexpr bool conjunction_v = conjunction<_Traits...>::value;
template <class _Trait> struct negation : bool_constant<!static_cast<bool>(_Trait::value)> {};
template <class... _Types> using void_t = void;
template <class _Ty, class = void> struct _Add_reference {
    using _Lvalue = _Ty;
    using _Rvalue = _Ty;
};
template <class _Ty> struct _Add_reference<_Ty, void_t<_Ty &>> {
    using _Lvalue = _Ty &;
    using _Rvalue = _Ty &&;
};
template <class _Ty> struct add_lvalue_reference { using type = typename _Add_reference<_Ty>::_Lvalue; };
template <class _Ty> using add_lvalue_reference_t = typename _Add_reference<_Ty>::_Lvalue;
template <class _To, class _From> struct is_assignable : bool_constant<__is_assignable(_To, _From)> {};
template <class _To, class _From> inline constexpr bool is_assignable_v = __is_assignable(_To, _From);
template <class _Ty>
struct is_copy_assignable
    : bool_constant<__is_assignable(add_lvalue_reference_t<_Ty>, add_lvalue_reference_t<const _Ty>)> {};
template <class _Ty>
inline constexpr bool is_copy_assignable_v = __is_assignable(add_lvalue_reference_t<_Ty>,
                                                             add_lvalue_reference_t<const _Ty>);
template <class _Ty> struct is_move_assignable : bool_constant<__is_assignable(add_lvalue_reference_t<_Ty>, _Ty)> {};
template <class _Ty> inline constexpr bool is_move_assignable_v = __is_assignable(add_lvalue_reference_t<_Ty>, _Ty);
template <class _Ty> struct _Identity { using type = _Ty; };
template <class _Ty> using _Identity_t = typename _Identity<_Ty>::type;

template <class...> struct tuple;
template <class _Tuple> struct tuple_size;
template <class _Tuple, class = void> struct _Tuple_size_sfinae {};
template <class _Tuple>
struct _Tuple_size_sfinae<_Tuple, void_t<decltype(tuple_size<_Tuple>::value)>>
    : integral_constant<size_t, tuple_size<_Tuple>::value> {};
template <class _Tuple> struct tuple_size<const _Tuple> : _Tuple_size_sfinae<_Tuple> {};
template <class _Tuple> struct tuple_size<volatile _Tuple> : _Tuple_size_sfinae<_Tuple> {};
template <class _Tuple> struct tuple_size<const volatile _Tuple> : _Tuple_size_sfinae<_Tuple> {};
template <class _Ty> inline constexpr size_t tuple_size_v = tuple_size<_Ty>::value;
template <class... _Types> struct tuple_size<tuple<_Types...>> : integral_constant<size_t, sizeof...(_Types)> {};
template <bool _Same, class _Dest, class... _Srcs> struct _Tuple_assignable_val0 : false_type {};
template <class... _Dests, class... _Srcs>
struct _Tuple_assignable_val0<true, tuple<_Dests...>, _Srcs...>
    : bool_constant<conjunction_v<is_assignable<_Dests &, _Srcs>...>> {};
template <class _Dest, class... _Srcs>
inline constexpr bool _Tuple_assignable_v =
    _Tuple_assignable_val0<tuple_size_v<_Dest> == sizeof...(_Srcs), _Dest, _Srcs...>::value;
template <class _Dest, class... _Srcs>
struct _Tuple_assignable_val : bool_constant<_Tuple_assignable_v<_Dest, _Srcs...>> {};
template <class... _Types> struct tuple;
template <> struct tuple<> {};
template <class _This, class... _Rest> struct tuple<_This, _Rest...> : private tuple<_Rest...> {
    tuple &operator=(const volatile tuple &) = delete;
    template <class _Myself = tuple, class _This2 = _This,
              enable_if_t<conjunction_v<is_copy_assignable<_This2>, is_copy_assignable<_Rest>...>, int> = 0>
    tuple &operator=(_Identity_t<const _Myself &> _Right) {
        return *this;
    }
    template <class _Myself = tuple, class _This2 = _This,
              enable_if_t<conjunction_v<is_move_assignable<_This2>, is_move_assignable<_Rest>...>, int> = 0>
    tuple &operator=(_Identity_t<_Myself &&>) {
        return *this;
    }
    template <class... _Other, enable_if_t<conjunction_v<negation<is_same<tuple, tuple<_Other...>>>,
                                                         _Tuple_assignable_val<tuple, const _Other &...>>,
                                           int> = 0>
    tuple &operator=(const tuple<_Other...> &) {
        return *this;
    }
    template <
        class... _Other,
        enable_if_t<conjunction_v<negation<is_same<tuple, tuple<_Other...>>>, _Tuple_assignable_val<tuple, _Other...>>,
                    int> = 0>
    tuple &operator=(tuple<_Other...> &&) {
        return *this;
    }
    _This _Myfirst;
};

template <class X> struct inner {};
template <class... Args> struct container { tuple<inner<Args>...> _data; };
int main() {
    static_assert(is_move_assignable_v<container<int>>);
    static_assert(is_copy_assignable_v<container<int>>);
}
