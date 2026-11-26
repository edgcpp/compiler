//type:cp
//options_all:--microsoft_v 1940 --ms_c++20
constexpr bool test() {
    int c[2] = { 2, 3 };
    static_assert(&c[0] <= &c[1]);
    return true;
}

static_assert(test());

int main()
{
    return 0;
}
