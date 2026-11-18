//options_all:--microsoft --c++14
int g(int(*pf)(int) throw())
{
return 0;
}

void test()
{
int (&pf2)(int(*pf)(int) noexcept) = g;

}
