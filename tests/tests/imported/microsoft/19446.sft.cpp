//options_all:--c++17
template<typename T>
struct A
{
    template<typename U>
    static constexpr int V = 1;
};
template<>
template<typename U>
constexpr int A<int>::V = 2;
