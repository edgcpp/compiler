//options_all:--c++11
template <class T>
struct S {
    [[noreturn]] friend int f() { while(1); }
};
