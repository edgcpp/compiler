//type:fn
//options_all:--microsoft_v 1903
namespace NS
{
  template <typename>
  struct A
  {
    A(int) {}
  };
}

template <int>
struct B : NS::A<int>
{
  using A<int>::A;
};

void test()
{
    B<1> test = 5;
}
