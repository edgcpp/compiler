//options_all:--c++20
template <class T>
struct array { T data(); };

template <class T>
concept has_data = requires(T x) { x.data(); };

template <class T>
struct X {
    X(has_data auto &x) {}
};

//template <has_data T>
//X(T &x) -> X<decltype(x.data())>;

X(has_data auto &x) -> X<decltype(x.data())>;

void f(array<int> v) {
    auto x = X(v);
}
