//type:cp
//options_all:--microsoft_v 1944 --ms_c++20
struct in_place_t {
};
inline constexpr in_place_t in_place{};

template <class _Ty>
struct remove_reference {
    using type = _Ty;
};

template <class _Ty>
using remove_reference_t = typename remove_reference<_Ty>::type;

template <class _Ty>
[[msvc::intrinsic]] constexpr _Ty&& forward(remove_reference_t<_Ty>& _Arg) noexcept {
    return static_cast<_Ty&&>(_Arg);
}

template <class _Ty>
[[nodiscard]] constexpr _Ty* addressof(_Ty& _Val) noexcept {
    return __builtin_addressof(_Val);
}

template <class _Ty>
struct remove_cv {
    using type = _Ty;
};

template <class _Ty>
using remove_cv_t = typename remove_cv<_Ty>::type;

template <class _Ty>
class _Movable_box {
public:
    template <class... _Types>
    constexpr _Movable_box(in_place_t, _Types&&... _Args)
        : _Val(forward<_Types>(_Args)...) {
    }

    constexpr _Movable_box& operator=(const _Movable_box& _That)
    {
        static_cast<_Ty&>(_Val) = static_cast<const _Ty&>(_That._Val);
        return *this;
    }

    constexpr _Ty& operator*() noexcept {
        return _Val;
    }

private:
    remove_cv_t<_Ty> _Val;
};

class _Cpo {
public:
    template <class _Ty>
    constexpr auto operator()(_Ty&& _Val) const
    {
        return _Val.begin();
    }
};

inline constexpr _Cpo begin;

template <class _Derived>
class view_interface {
private:
    constexpr _Derived& _Cast() noexcept {
        return static_cast<_Derived&>(*this);
    }
public:
    constexpr decltype(auto) front()
    {
        auto& _Self = _Cast();
        return *begin(_Self);
    }
};

template <class _Ty>
class single_view : public view_interface<single_view<_Ty>> {
public:
    template <class... _Types>
    constexpr explicit single_view(in_place_t, _Types&&... _Args)
        : _Val{ in_place, forward<_Types>(_Args)... } {
    }
    constexpr _Ty* begin() noexcept {
        return data();
    }
    constexpr _Ty* end() noexcept {
        return data() + 1;
    }
    constexpr _Ty* data() noexcept {
        return addressof(*_Val);
    }
    constexpr const _Ty* data() const noexcept {
        return addressof(*_Val);
    }
private:
    _Movable_box<_Ty> _Val{};
};

struct ConstSelection {
    int value = 0;
};

constexpr bool test_cv() {
    single_view<ConstSelection> svx{ in_place, 0 };
    single_view<ConstSelection> svy{ in_place, 42 };
    svy = svx;
    return svy.front().value == 0;
}

int main() {
    static_assert(test_cv());
}
