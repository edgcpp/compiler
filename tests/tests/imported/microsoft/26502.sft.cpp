//type:cp
//options_all:--ms_c++20 --microsoft_v 1940
struct A {
    enum Terminal { T_EQUAL };
};
struct B : A {
    using enum Terminal;
};
struct C : B {
    void TestBody() { T_EQUAL; }
};
