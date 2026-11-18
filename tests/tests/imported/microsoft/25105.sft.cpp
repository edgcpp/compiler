//options_all:--c++20
void f(const auto &v) requires requires { v.x(); } { v.x(); }
