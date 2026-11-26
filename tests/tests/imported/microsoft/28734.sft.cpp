//type:cp
//options_all:-w --microsoft_v 1950 --ms_c++20
typedef __int64 ptrdiff_t;

namespace std
{
    template <class, class>
    constexpr bool is_same_v = false;
    template <class _Ty>
    constexpr bool is_same_v<_Ty, _Ty> = true;

    template <class _Ty>
    _Ty&& declval() noexcept
    {
        static_assert(false, "Calling declval is ill-formed, see N4950 [declval]/2.");
    }

    template <class _Ty1, class _Ty2>
    concept _Same_impl =
        is_same_v<_Ty1, _Ty2>;
    template <class _Ty1, class _Ty2>
    concept same_as = _Same_impl<_Ty1, _Ty2> && _Same_impl<_Ty2, _Ty1>;

    template <class _Ty>
    using _With_reference = _Ty &;
    template <class _Ty>
    concept _Can_reference = requires { typename _With_reference<_Ty>; };
    template <class _Ty>
    concept _Dereferenceable = requires(_Ty &__t) {
        { *__t } -> _Can_reference;
    };

    template <_Dereferenceable _Ty>
    using iter_reference_t = decltype(*::std::declval<_Ty &>());

    template <class _It>
    concept contiguous_iterator = requires(_It __i, const _It __j, const ptrdiff_t __n) {
        { __j[__n] } -> same_as<iter_reference_t<_It>>;
    };

    template <class _Iter1, class _Iter2>
    constexpr bool _Iterators_are_contiguous = contiguous_iterator<_Iter1> && contiguous_iterator<_Iter2>;

    template <class _Source, class _Dest, class _SourceRef, class _DestRef>
    struct _Trivial_cat
    {
        static constexpr bool _Bitcopy_assignable = is_same_v<_DestRef, decltype(::std::declval<_DestRef>() = ::std::declval<_SourceRef>())>;
    };

    template <class _SourceIt, class _DestIt, bool _Are_contiguous = _Iterators_are_contiguous<_SourceIt, _DestIt>>
    struct _Iter_copy_cat
        : _Trivial_cat<_SourceIt, _DestIt, iter_reference_t<_SourceIt>, iter_reference_t<_DestIt>>
    {
    };

    template <class _InIt, class _SizeTy, class _OutIt>
    constexpr _OutIt _Copy_n_unchecked4(_InIt _First, _SizeTy _Count, _OutIt _Dest)
    {
        if constexpr (_Iter_copy_cat<_InIt, _OutIt>::_Bitcopy_assignable)
        {
            return _First;
        }
        return _Dest;
    }

    struct to_chars_result
    {
        char *ptr;
    };

    template <class _RawTy>
    [[nodiscard]] inline to_chars_result _Integer_to_chars(
        char *_First, char *const _Last, const _RawTy _Raw_value, const int _Base) noexcept
    {
        char _Buff[8];
        char *const _Buff_end = _Buff + 8;

        const ptrdiff_t _Digits_written = 0;
        if (_Last - _First < _Digits_written)
        {
            return {_Last};
        }
        _Copy_n_unchecked4(_Buff_end, _Digits_written, _First);
        return {_First + _Digits_written};
    }
    inline to_chars_result to_chars(
        char *const _First, char *const _Last, const char _Value, const int _Base = 10) noexcept
    {
        return _Integer_to_chars(_First, _Last, _Value, _Base);
    }
}

int main()
{
    return 0;
}
