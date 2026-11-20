//type:fn
//options_all:--microsoft_v 1928 --concepts
template <class T>
struct X {
    template <class U> requires requires { f(U()); }
    X(U u) {}
};

X<int> r{ 1 };
