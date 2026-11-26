//type:fn
//options_all:--c++20
struct X {
    explicit constexpr X(int) {}; // Cannot be copy-initialized
};

struct S {
    X x;
};

S ss1 { {3} };
