//type:fp
//options_all:--microsoft_version 1910
template<class T>
struct Base {
    T member;
    Base() noexcept = default;
    explicit constexpr Base(T val) : member(val) {}
    Base(const Base&) = delete;
    Base& operator=(const Base&) = delete;
};

template<class T>
struct Derived : Base<T> {
    using Base<T>::Base;
};

struct i_have_no_constexpr_constructor {
    int id;
    i_have_no_constexpr_constructor() : id(0) {}
    i_have_no_constexpr_constructor(const i_have_no_constexpr_constructor&) = default;
};

i_have_no_constexpr_constructor get_instance() { return {}; }

int main() {
    Derived<i_have_no_constexpr_constructor> instance(get_instance());
    (void)instance;
}
