//type: rp
//options: --c++26:--c++26 -tused
//remark:[GH #216] Lifetimes of temporaries in expansion statements.

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

namespace destructuring_initializer_temporaries
{
  using initializer_temporaries::S;

  struct W { S s; };

  constexpr bool f()
  {
    int live = 0;
    template for (const auto& x : W{S(live)})
    {
      if (live != 1 || x.get() != 1) return false;
    }
    return live == 0;
  }
  static_assert(f());
}

namespace tuple_binding_lifetimes
{
  struct X
  {
    int& destroyed;
    int id;
    constexpr ~X() { destroyed = 10 * destroyed + id; }
  };

  struct T { int& destroyed; };

  template<unsigned long I> constexpr X get(const T& t)
  {
    return {t.destroyed, I == 0 ? 1 : 2};
  }
}

namespace std
{
  template<typename T> struct tuple_size;
  template<unsigned long I, typename T> struct tuple_element;
  template<> struct tuple_size<tuple_binding_lifetimes::T>
  {
    static constexpr unsigned long value = 2;
  };
  template<unsigned long I> struct tuple_element<I, tuple_binding_lifetimes::T>
  {
    using type = tuple_binding_lifetimes::X;
  };
}

namespace tuple_binding_lifetimes
{
  constexpr int f(bool stop)
  {
    int destroyed = 0;
    bool copies_destroyed_in_order = true;
    T t{destroyed};
    template for (auto x : t)
    {
      /* Only the previous element's copy can have been destroyed here. */
      copies_destroyed_in_order &= destroyed == (x.id == 1 ? 0 : 3);
      x.id += 2;
      if (stop) break;
    }
    /* Element copies die at the end of each element block; the get results
       die in reverse order when the expansion scope is exited. */
    return copies_destroyed_in_order ? destroyed : -1;
  }
  static_assert(f(false) == 3421);
  static_assert(f(true) == 321);
}

int main()
{
  return !initializer_temporaries::f() || !indirect_reference::f() ||
         !instantiated_initializer::f<initializer_temporaries::S>() ||
         !destructuring_initializer_temporaries::f() ||
         tuple_binding_lifetimes::f(false) != 3421 ||
         tuple_binding_lifetimes::f(true) != 321;
}
