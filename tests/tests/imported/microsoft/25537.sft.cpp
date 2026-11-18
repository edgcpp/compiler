//options_all:--c++20
template <int>
struct fixed_string {};

template <int>
void select() {}

template <fixed_string>
void select() {}

void f() { select<0>(); }
