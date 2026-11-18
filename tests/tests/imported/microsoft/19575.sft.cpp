//type:fn
//options_all:--microsoft_version 1914
//options:--ms_c++14:--ms_c++17
struct B1 {
    B1(double);
};

struct B2 {
    B2(double);
};

struct D1 : B1, B2 {
    using B1::B1;
    using B2::B2;
};

D1 d(1.0);
