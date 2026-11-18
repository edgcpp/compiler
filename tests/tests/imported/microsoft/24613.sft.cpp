//options_all:--c++14
template <class>
struct Expr {};

template <int N, class ...Ts, template <int, class ...> class T>
struct Expr<T<N, Ts...>> {
    template <class ...Us, template <int, class ...> class U>
    void f(Expr<U<N, Us...>> &);
};

template <int N>
class Field : public Expr<Field<N>> {};

void f(Field<2> u, Field<2> v) {
    u.f(v);
}
