//type:fn
//options_all:--c++17
template <class T> struct A {
    template <class U> struct U {};
};

template <class T> struct A {
    template <class U> inline int U(U) {return -47;}
};
int i = A<double>::U(-23);
