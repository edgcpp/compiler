//type:fp
//options_all:--c++11
struct Base {};
struct Derived : Base {
constexpr Derived() = default;
};
