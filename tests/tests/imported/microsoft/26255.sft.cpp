//options_all:--c++20
//type:fp
constexpr int foo() {
    constexpr int arr[]{ 1,2,3 };
    for (int i = 0; auto _ : arr) { i++; }
    return 0;
}

constexpr auto a = foo();
