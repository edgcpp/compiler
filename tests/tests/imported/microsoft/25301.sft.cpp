//type:fn
//options_all:--c++17
template <class T>
struct S {
    template <class U>
    S(U &&u) {}
};

template <class U>
S(U &&) -> S<U>;

template <class U>
S(U &&) -> S<U>;
