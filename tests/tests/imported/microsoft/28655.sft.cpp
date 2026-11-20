//type:cp
//options_all:--c++23
struct foo
{
    auto bar(this auto &&self)
    {
        return 1;
    }

    operator int(this auto &&self)
    {
        return 1;
    }
};

int main()
{
    int bar = foo{};

    return foo{}.bar();
}
