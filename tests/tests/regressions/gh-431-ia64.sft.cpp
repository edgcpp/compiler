//type:fp
//options:--c++11:--gnu_version 150200:--clang_version 220100:--ms_c++20 --microsoft_version 1950
//options_all:-tused
//requires:IA64_ABI=1

namespace in_class
{
  template<int N>
  struct C
  {
    virtual void f()
    {
      static_assert(N != 1, "");
    }
  };

  extern template class C<1>;

  C<1> c;
}

namespace external_linkage
{
  struct External {};

  template<class T> struct C
  {
    C() {}
    ~C() {}

    virtual void f()
    {
      static_assert(sizeof(T) == 0, "");
    }
  };

  extern template struct C<External>;

  /* Both user-defined bodies are checked, but the vtable is suppressed. */
  void g()
  {
    C<External> c;
  }
}

namespace partial_specialization
{
  template<class T> struct C;

  template<class T> struct C<T*>
  {
    C() {}
    virtual ~C() {}

    virtual void f()
    {
      static_assert(sizeof(T) == 0, "");
    }
  };

  extern template struct C<int*>;

  /* A partial specialization is still subject to extern template. */
  void g()
  {
    C<int*> c;
  }
}

#if defined(__GNUC__) || defined(__clang__)
namespace hidden_external_linkage
{
  template<class T> struct __attribute__((visibility("hidden"))) C
  {
    virtual void f()
    {
      static_assert(sizeof(T) == 0, "");
    }
  };

  extern template struct C<int>;

  /* Hidden visibility is not internal linkage: suppress this vtable too. */
  C<int> c;
}
#endif

namespace inline_out_of_class
{
  template<int N>
  struct C
  {
    virtual void f();
  };

  template<int N>
  inline void C<N>::f()
  {
    static_assert(N != 1, "");
  }

  extern template class C<1>;

  C<1> c;
}
