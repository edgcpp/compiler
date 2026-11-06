//type: fn
//options: --c++23:--c++23 --gn 160100;fp
//remark:[GH #216] Expansion statements require C++26, except as a GNU extension.

void f()
{
  template for (auto v : { 1 }) { (void)v; }
}
