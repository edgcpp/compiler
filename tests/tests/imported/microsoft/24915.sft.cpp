//type:fn
//options_all:--microsoft_v 1920 --ms_c++14
namespace ns {}

struct A {
    template <class T>
    static constexpr bool foo = ns::does_not_exist<int, T>;
};
