//options_all:--c++11 --microsoft
//type:fp
template <size_t N>
void f(long(&&arr)[N])
{
    (void)arr;
}
int main()
{
    f({1, 2});
}
