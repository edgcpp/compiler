//options_all:--c++20
template <class T>
auto coerce(T &&) -> T && requires true {}
