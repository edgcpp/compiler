//type:fn
//options_all:--c++11
namespace ns {}

struct A {
    template <class T>
    static constexpr bool foo = ns::does_not_exist<int, T>;
};
