//type:cp
//options_all:--microsoft_v 1940 --ms_c++20
struct A
{
    explicit A() = default;
};

struct B
{
    A a;
};

void foo()
{
    B b{ .a{} };
}

int main()
{
    return 0;
}
