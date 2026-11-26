//options_all:--c++20
struct Base { void func() {} };
struct Derived : private Base {
    using Base::func;
};

template <class T>
concept HasFunc = requires (T t) { t.func(); };

static_assert(HasFunc<Derived>);
