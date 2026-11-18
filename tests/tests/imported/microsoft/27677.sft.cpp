//type:cp
//options:--microsoft_v 1940 --ms_c++20:--c++20
template <class _Ty>
struct remove_reference {
    using type = _Ty;
    using _Const_thru_ref_type = const _Ty;
};

template <class _Ty>
struct remove_reference<_Ty&> {
    using type = _Ty;
    using _Const_thru_ref_type = const _Ty&;
};

template <class _Ty>
using remove_reference_t = typename remove_reference<_Ty>::type;

template <class _Ty>
constexpr remove_reference_t<_Ty>&& move(_Ty&& _Arg) noexcept {
    return static_cast<remove_reference_t<_Ty>&&>(_Arg);
}

template <class _Ty, class _Dx_noref, class = void>
struct _Get_deleter_pointer_type {
    using type = _Ty*;
};

template <class _Ty>
struct default_delete {
};

template <class _Ty, class _Dx = default_delete<_Ty>>
class unique_ptr {
public:
    using pointer = typename _Get_deleter_pointer_type<_Ty, remove_reference_t<_Dx>>::type;
    explicit unique_ptr(pointer _Ptr) noexcept {}
    unique_ptr(unique_ptr&& _Right) noexcept {}
    unique_ptr(const unique_ptr&) = delete;
};

class MyClass {
public:
    MyClass(unique_ptr<int> t) : self(move(t)) {}
    unique_ptr<int> self;
};

static MyClass func() {
    int v = 0;
    auto p = unique_ptr<int>(new int(v));
    return p;
}

int main()
{
    return 0;
}
