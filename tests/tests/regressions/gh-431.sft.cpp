//type:fp
//options:--c++11:--gnu_version 150200:--clang_version 220100:--ms_c++20 --microsoft_version 1950
//options_all:-tused

namespace out_of_class
{
  template<int N>
  struct C
  {
    virtual void f();
  };

  template<int N>
  void C<N>::f()
  {
    static_assert(N != 1, "");
  }

  extern template class C<1>;

  C<1> c;
}

namespace constant_evaluation
{
  template<class T>
  struct C
  {
    static constexpr int f()
    {
      return 42;
    }

    constexpr int g() const
    {
      return 43;
    }
  };

  extern template struct C<int>;

  static_assert(C<int>::f() == 42, "");
  static_assert(C<int>{}.g() == 43, "");
}
