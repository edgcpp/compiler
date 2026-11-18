//type:cp
//options_all:--clang_v 190000 --c++20
typedef decltype((char*)0-(char*)0) ptrdiff_t;
typedef decltype(sizeof(0)) size_t;

namespace std {
    template <class _Ptr>
    struct pointer_traits {};
    template <class _Tp>
    struct pointer_traits<_Tp*> {
        typedef _Tp* pointer;
        typedef _Tp element_type;
        typedef ptrdiff_t difference_type;
    public:
        constexpr static pointer
            pointer_to(element_type& __r) noexcept {
            return __builtin_addressof(__r);
        }
    };

    template <class _Tp>
    using __make_unsigned_t = __make_unsigned(_Tp);
    template <class _Tp>
    struct make_unsigned {
        using type = __make_unsigned_t<_Tp>;
    };

    template <class _Tp>
    inline constexpr size_t __constexpr_strlen(const _Tp* __str) noexcept {
        size_t __i = 0;
        for (; __str[__i] != '\0'; ++__i)
            ;
        return __i;
    }

    enum class __element_count : size_t {};

    template <class _Tp, class _Up>
    constexpr _Tp* __constexpr_memmove(_Tp* __dest, _Up* __src, __element_count __n) {
        size_t __count = static_cast<size_t>(__n);
        ::__builtin_memmove(__dest, __src, __count * sizeof(_Tp));
        return __dest;
    }

    template <class _Tp>
    class allocator;
    template <class _CharT>
    struct char_traits;
    template <>
    struct char_traits<char>;
    template <class _CharT, class _Traits = char_traits<_CharT>, class _Allocator = allocator<_CharT> >
    class basic_string;
    using string = basic_string<char>;

    template <class _Tp,
        class _Alloc,
        class _RawAlloc = _Alloc,
        bool = false>
    struct __pointer {
        using type = typename _RawAlloc::pointer;
    };
    template <class _Tp, class _Alloc, class _RawAlloc>
    struct __pointer<_Tp, _Alloc, _RawAlloc, false> {
        using type = _Tp*;
    };

    template <class _Alloc, class _DiffType>
    struct __size_type : make_unsigned<_DiffType> {};

    template <class _Alloc, class _Ptr>
    struct __alloc_traits_difference_type {
        using type = typename pointer_traits<_Ptr>::difference_type;
    };

    template <class _Alloc>
    struct allocator_traits {
        using allocator_type = _Alloc;
        using value_type = typename allocator_type::value_type;
        using pointer = typename __pointer<value_type, allocator_type>::type;
        using difference_type = typename __alloc_traits_difference_type<allocator_type, pointer>::type;
        using size_type = typename __size_type<allocator_type, difference_type>::type;
    };

    template <class _Tp>
    class allocator {
    public:
        typedef _Tp value_type;
    };

    struct __default_init_tag {};
    struct __value_init_tag {};

    template <class _Tp, int _Idx, bool _CanBeEmptyBase = false>
    struct __compressed_pair_elem {
        using reference = _Tp&;
        constexpr explicit __compressed_pair_elem(__default_init_tag) {}
        constexpr explicit __compressed_pair_elem(__value_init_tag) : __value_() {}
        constexpr reference __get() noexcept { return __value_; }
    private:
        _Tp __value_;
    };

    template <class _T1, class _T2>
    class __compressed_pair : private __compressed_pair_elem<_T1, 0>, private __compressed_pair_elem<_T2, 1> {
    public:
        using _Base1 = __compressed_pair_elem<_T1, 0>;
        using _Base2 = __compressed_pair_elem<_T2, 1>;

        template <class _U1, class _U2>
        constexpr explicit __compressed_pair(_U1&& __t1, _U2&& __t2)
            : _Base1(__t1), _Base2(__t2) {
        }
        constexpr typename _Base1::reference first() noexcept {
            return static_cast<_Base1&>(*this).__get();
        }
    };

    template <>
    struct char_traits<char> {
        using char_type = char;
        static inline size_t constexpr length(const char_type* __s) noexcept {
            return __constexpr_strlen(__s);
        }
        static inline constexpr char_type* copy(char_type* __s1, const char_type* __s2, size_t __n) noexcept {
            __constexpr_memmove(__s1, __s2, __element_count(__n));
            return __s1;
        }
    };

    template <class _CharT, class _Traits, class _Allocator>
    class basic_string {
    public:
        typedef _Traits traits_type;
        typedef _CharT value_type;
        typedef _Allocator allocator_type;
        typedef allocator_traits<allocator_type> __alloc_traits;
        typedef typename __alloc_traits::size_type size_type;
        typedef typename __alloc_traits::pointer pointer;
    private:
        struct __long {
            pointer __data_;
            size_type __size_;
            size_type __cap_ : sizeof(size_type) * 8 - 1;
            size_type __is_long_ : 1;
        };
        enum { __min_cap = (sizeof(__long) - 1) / sizeof(value_type) > 2 ? (sizeof(__long) - 1) / sizeof(value_type) : 2 };
        struct __short {
            value_type __data_[__min_cap];
            unsigned char __padding_[sizeof(value_type) - 1];
            unsigned char __size_ : 7;
            unsigned char __is_long_ : 1;
        };
        union __rep {
            __short __s;
            __long __l;
        };
        __compressed_pair<__rep, allocator_type> __r_;
    public:
        constexpr basic_string()
            : __r_(__value_init_tag(), __default_init_tag()) {
        }

        constexpr basic_string(const _CharT* __s)
            : __r_(__default_init_tag(), __default_init_tag()) {
            __init(__s, traits_type::length(__s));
        }

    private:
        inline constexpr void __init(const value_type* __s, size_type __sz);
    };
    template <class _CharT, class _Traits, class _Allocator>
    constexpr void
        basic_string<_CharT, _Traits, _Allocator>::__init(const value_type* __s, size_type __sz) {
        pointer __p = pointer_traits<pointer>::pointer_to(__r_.first().__s.__data_[0]);
        traits_type::copy(__p, __s, __sz);
    }
}
template <class _Tp, unsigned long long _Size>
struct array {
    _Tp __elems_[_Size];
};
template <unsigned long long a> struct b :array<std::string, a>
{
};
b<3> const c = { "", "" };
int main() { return 0; }
