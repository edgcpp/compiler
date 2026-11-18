//type:cp
//options_all:--ms_c++20 --microsoft_v 1950
// vvv just for reducing standard library implementation vvv

using size_t = decltype(sizeof(0));
using ptrdiff_t = decltype(static_cast<int*>(nullptr) - static_cast<int*>(nullptr));

[[nodiscard]] inline void* __cdecl operator new(size_t, void* _Ptr) noexcept {
    return _Ptr;
}

inline void __cdecl operator delete(void*, void*) noexcept {}

[[nodiscard]] inline void* __cdecl operator new[](size_t, void* _Ptr) noexcept {
    return _Ptr;
}

inline void __cdecl operator delete[](void*, void*) noexcept {}

namespace std {
    template <class _Ty, _Ty _Val>
    struct integral_constant {
        static constexpr _Ty value = _Val;

        using value_type = _Ty;
        using type = integral_constant;

        constexpr operator value_type() const noexcept {
            return value;
        }

        [[nodiscard]] constexpr value_type operator()() const noexcept {
            return value;
        }
    };

    using true_type = integral_constant<bool, true>;

    template <class>
    constexpr bool is_array_v = false;
    template <class _Ty, size_t _Nx>
    constexpr bool is_array_v<_Ty[_Nx]> = true;
    template <class _Ty>
    constexpr bool is_array_v<_Ty[]> = true;

    template <class>
    constexpr bool is_unbounded_array_v = false;
    template <class _Ty>
    constexpr bool is_unbounded_array_v<_Ty[]> = true;

    template <class _Ty, class... _Types>
        requires (!is_unbounded_array_v<_Ty>) && requires(_Ty* _Ptr, _Types&&... _Args) {
        ::new (static_cast<void*>(_Ptr)) _Ty(static_cast<_Types&&>(_Args)...); // per LWG-3888
    }
    constexpr _Ty* construct_at(_Ty* const _Ptr, _Types&&... _Args) noexcept(
        noexcept(::new (static_cast<void*>(_Ptr)) _Ty(static_cast<_Types&&>(_Args)...))) {
        if constexpr (is_array_v<_Ty>) {
            static_assert(sizeof...(_Types) == 0);
            ::new (static_cast<void*>(_Ptr)) _Ty();
            return __builtin_launder(_Ptr); // per old resolution of LWG-3436
        }
        else {
            return ::new (static_cast<void*>(_Ptr)) _Ty(static_cast<_Types&&>(_Args)...);
        }
    }

    template <class _Ty>
    constexpr void destroy_at(_Ty* const _Ptr) noexcept {
        if constexpr (is_array_v<_Ty>) {
            for (auto& _Elem : *_Ptr) {
                ::std::destroy_at(__builtin_addressof(_Elem));
            }
        }
        else {
            _Ptr->~_Ty();
        }
    }

    template <class _Ty>
    class allocator {
    public:
        using value_type = _Ty;

        using size_type = size_t;
        using difference_type = ptrdiff_t;

        using propagate_on_container_move_assignment = true_type;
        using is_always_equal [[deprecated]] = true_type;

        constexpr allocator() noexcept {}

        constexpr allocator(const allocator&) noexcept = default;
        template <class _Other>
        constexpr allocator(const allocator<_Other>&) noexcept {}
        constexpr ~allocator() = default;
        constexpr allocator& operator=(const allocator&) = default;

        constexpr void deallocate(_Ty* const _Ptr, size_t) noexcept {
            ::operator delete(_Ptr);
        }

        [[nodiscard]] constexpr _Ty* allocate(const size_t _Count) {
            const size_t _Byte_count = size_t(-1) / sizeof(_Ty) <= _Count ? size_t(-1) : sizeof(_Ty) * _Count;
            return static_cast<_Ty*>(::operator new(_Byte_count));
        }
    };

    template <class _Tx, class _Ty>
    [[nodiscard]] constexpr bool operator==(const allocator<_Tx>&, const allocator<_Ty>&) noexcept {
        return true;
    }
} // namespace std

// ^^^ just for reducing standard library implementation ^^^

class Trivial {};

class Nontrivial {
public:
    constexpr Nontrivial() : p_{ std::allocator<Trivial>{}.allocate(1) } {
        std::construct_at(p_);
    }
    constexpr ~Nontrivial() {
        std::destroy_at(p_);
        std::allocator<Trivial>{}.deallocate(p_, 1);
    }

    Nontrivial(const Nontrivial&) = delete;
    Nontrivial& operator=(const Nontrivial&) = delete;

private:
    Trivial* p_;
};

constexpr bool test() {
    constexpr size_t arr_len = 42; // crashes if arr_len > 1
    {
        union U {
            constexpr U() {}
            constexpr ~U() {}

            Nontrivial a[arr_len];
        };
        U u;
        std::construct_at(&u.a);
        std::destroy_at(&u.a);
    }

    return true;
}
static_assert(test());
