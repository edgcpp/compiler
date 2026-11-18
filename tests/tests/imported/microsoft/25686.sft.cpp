//options_all:--c++20
template <int ...i>
inline constexpr bool t = sizeof...(i) != 0;

template <int ...i> requires (t<i...>)
struct A;

template <int ...i> requires (t<i...>)
struct A {};

int main() {
    A<0, 1> a{};
}
