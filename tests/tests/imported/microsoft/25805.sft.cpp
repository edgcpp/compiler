//type:cp
//options_all:--microsoft_v 1938 --c++20
namespace std {

template <class _Ty>
struct allocator {
    constexpr _Ty* allocate(const size_t _Count) {
        return static_cast<_Ty*>(::operator new(_Count * sizeof(_Ty)));
    }

    constexpr void deallocate(_Ty* const _Ptr, const size_t _Count) {
        ::operator delete(_Ptr);
    }
};

}

constexpr bool test() {
    auto _Al = std::allocator<int>();
    auto _Myfirst = _Al.allocate(1);
    auto it = _Myfirst;
    it += 1;
    _Al.deallocate(_Myfirst, 1);
    return true;
}

static_assert(test());
