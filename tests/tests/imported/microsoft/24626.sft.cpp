//options_all:--c++14
template <class _Derived>
struct _Base {
    constexpr auto foo() && {
        return static_cast<_Derived &&>(*this);
    }
};

struct _Partial : _Base<_Partial> {};

constexpr auto trim_back = _Partial{}.foo();
