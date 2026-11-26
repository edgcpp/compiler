//options_all:--microsoft --c++14
template <typename> inline
int returns_int()
{
  return 5;
}
template <typename T>
struct test2
{
  typedef decltype(returns_int<T>()) type;
};
template <typename>
struct test1
{
  using type = int;
};
void test_test2()
{
  auto lambda = [](){return 5;};
  using test2_type = typename test2<decltype(lambda)()>::type;
  using test2_type2 = typename test1<test2_type>::type;
}
void test_test1()
{
  using test1_type = typename test1<int>::type;
}
