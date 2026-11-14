//type: rp
//options: --c++26:--c++26 -tused
//remark:[GH #216] Enumerating initializer temporaries survive each element.

namespace initializer_temporaries
{
  struct S
  {
    int& live;
    constexpr S(int& n) : live(n) { ++live; }
    constexpr ~S() { --live; }
    constexpr int get() const { return live; }
  };

  constexpr const S& identity(const S& s) { return s; }

  bool f()
  {
    int live = 0;
    /* [class.temporary]/8 extends S even though the iterator is an int.
       Each element's temporary dies before the next element starts. */
    template for (int x : { S(live).get(), S(live).get() })
    {
      if (live != 1 || x != 1) return false;
    }
    return live == 0;
  }
}

namespace indirect_reference
{
  using initializer_temporaries::S;
  using initializer_temporaries::identity;

  constexpr bool f()
  {
    int live = 0;
    /* A call returning a reference does not ordinarily extend its argument
       temporary's lifetime; the expansion-specific rule does. */
    template for (const auto& x : { identity(S(live)), identity(S(live)) })
    {
      if (live != 1 || x.get() != 1) return false;
    }
    return live == 0;
  }
  static_assert(f());
}

namespace instantiated_initializer
{
  template<typename T> bool f()
  {
    int live = 0;
    template for (auto x : { T(live).get(), T(live).get() })
    {
      if (live != 1 || x != 1) return false;
    }
    template for (auto&& x : { T(live), T(live) })
    {
      if (live != 1 || x.get() != 1) return false;
    }
    return live == 0;
  }
}

int main()
{
  return !initializer_temporaries::f() || !indirect_reference::f() ||
         !instantiated_initializer::f<initializer_temporaries::S>();
}
