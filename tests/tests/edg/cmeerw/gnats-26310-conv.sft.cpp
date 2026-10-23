//type:fp
//options:--c++11:--c++11:--c++11:--c++11
//options_all:--no_il_lower --il_display
//filter:awk '/^func-scope expr-node@/{f=1}/^$/{if (f) print $0; f=0}f' | grep -E -v '^(  start.|  end.|expr_range|next:)' | sed -e 's/@[0-9a-f]*//'

template<int I>
struct B : B<I - 1>
{ };

template<>
struct B<0>
{
  int i;
};

B<6> b6;

#if TEST_NUMBER == 1
void f(B<5> b5)
{
  const B<2> &b2 = b5;
}
#elif TEST_NUMBER == 2
void f(B<2> b2)
{
  const B<5> &b5 = static_cast<const B<5> &>(b2);
}
#elif TEST_NUMBER == 3
void f(int B<2>::*p2)
{
  const int B<5>::*p5 = p2;
}
#elif TEST_NUMBER == 4
void f(int B<5>::*p5)
{
  const int B<2>::*p2 = static_cast<const int B<2>::*>(p5);
}
#endif
