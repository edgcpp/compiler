//options_all:--c++17
struct A {
     A(int);
     A(const A &, int = 1);
};

struct C : A {
     using A::A;
};

C c1(1);
C c2(c1, 0);
