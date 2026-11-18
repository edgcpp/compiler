//type:cp
//options_all:--microsoft_v 1940 --ms_c++20
struct Inner
{
    explicit Inner(int) {}
};

struct Outer
{
    Inner inner;
};

int main()
{
    Outer outer
    {
        .inner{ 0 },
    };
}
