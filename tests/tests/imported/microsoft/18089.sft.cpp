//type:fp
//options_all:--microsoft_version 1910
template <typename T>
struct test
{
    static constexpr bool f() { return false; }
    static constexpr bool v = f();
};

const bool b = test<int>::v;
