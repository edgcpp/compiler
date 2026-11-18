//options_all:--microsoft --c++14
void f() throw() {}

void g()
{
                void (*pf1)() throw() = f;
                void (*pf2)() noexcept = f;
                pf1 = pf2;
}
