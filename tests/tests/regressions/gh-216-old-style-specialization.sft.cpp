//type:fp
//options:--microsoft --c++26
//remark:[GH #216] Expansion statement in a Microsoft old-style specialization.

namespace old_style_specialization
{
  template<typename T> struct A { void f(); };

  /* The body is checked here (there is no prototype instantiation of it). */
  void A<int>::f()
  {
    template for (auto x : { 1, 2 }) {}
  }
}

namespace old_style_specialization_lambda
{
  template<typename T> struct A { void f(); };

  void A<int>::f()
  {
    auto l = [] { template for (auto x : { 1, 2 }) {} };
    l();
  }
}
