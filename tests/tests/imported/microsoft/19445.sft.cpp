//options_all:--c++17
template <typename T>
auto L = [](auto) { return T(); };
int x = L<int>(1);
