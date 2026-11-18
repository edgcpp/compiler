//options_all:--microsoft --c++14
template <typename T, int N>
struct array
{
};
template <typename T>
struct numbers
{
    static const int size = 3;
    static const array<T, size> positive;
};
template <typename T>
const array<T, numbers<T>::size> numbers<T>::positive;
