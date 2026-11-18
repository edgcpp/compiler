//options_all:--c++14
struct C {
constexpr C(C *this_) : m(42), n(this_->m) {}
int m, n;
};
struct D {
C c;
constexpr D() : c(&c) {}
};
static_assert(D().c.n == 42, "");
