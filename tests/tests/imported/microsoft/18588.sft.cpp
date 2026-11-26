//type:fp
//options_all:--microsoft_version 1911
template <typename T, int N>
struct A {
    static constexpr int value = 123;
};

template <typename T, int N>
constexpr int A_v = A<T, N>::value;

template <typename T, int = A_v<T, 1>>
void f() { }

void g() {
    f<int>();
}
