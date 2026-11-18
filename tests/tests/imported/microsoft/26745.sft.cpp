//type:cp
//options_all:--microsoft_v 1940 --ms_c++latest
// Repros with prod/fe x86chk as of 2023-10-09.
// Affects STL test P2474R2_views_repeat.

// EDG rejects parenthesized aggregate initialization in a *mem-initializer*.

struct forward_tester {
    forward_tester() = default;
    constexpr forward_tester(const forward_tester&) {}
};

struct tuple_tester {
    forward_tester y;
};

struct t {
    t(const forward_tester& arg1)
        : val(arg1) // error: no instance of constructor "tuple_tester::tuple_tester" matches the argument list
    {}

    tuple_tester val;
};
