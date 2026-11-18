//options_all:--c++20
template <class R>
concept range = requires(R &r) { r.begin(); };

template <class T>
struct span {
    template <class R>
    span(R &&);
};

template <range R>
span(R &&) -> span<int>;

struct vector {
    int *begin();
};

void f(vector xs) {
    span s{ xs };
}
