//type: fn
//options: --c++26;fp:--c++26 --strict
//remark:[GH #216] Non-compound expansion statement bodies.

void f()
{
  template for (auto x : { 1 }) (void)x;
}
