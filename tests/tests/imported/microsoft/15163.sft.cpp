//options_all:--c++14 --microsoft
struct B {
B(int) noexcept{};
};

template<class ... T>
struct D : public T... {
using B::B;
};

void test()
{
D<B> dd(0);
}
