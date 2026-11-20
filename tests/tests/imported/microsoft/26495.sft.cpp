//type:cp
//options_all:--c++20
struct base128 {
    unsigned word;
    constexpr base128(unsigned x) : word{ x } {}
};

struct signed128 : base128 {
    using base128::base128;
};

template <class T>
concept integer_like = static_cast<T>(1).word == 1;

template <integer_like T>
void g(T) {}

void f(signed128 x) {
    g(x);
}
