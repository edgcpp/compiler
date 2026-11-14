//type: fn
//options: --c++03:--c++23:--c++23 --clang_version 220100:--ms_c++20 --microsoft_version 1950:--c++23 --gn 160100;fp:--c++26;fp
//remark:[GH #216] Expansion statements require C++26, except as a GNU extension.

void f()
{
  template for (auto v : { 1 }) { (void)v; }
}
