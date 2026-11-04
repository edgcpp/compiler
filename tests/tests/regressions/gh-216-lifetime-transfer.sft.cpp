//type: rp
//options: --c++26:--c++26 -tused
//remark:[GH #216] Initializer temporary cleanup on break and continue.

namespace transfer_cleanup
{
  struct S
  {
    int& live;
    S(int& n) : live(n) { ++live; }
    ~S() { --live; }
    int get() const { return live; }
  };

  bool f()
  {
    int live = 0, visits = 0;
    template for (int x : { S(live).get(), S(live).get() })
    {
      if (live != 1 || x != 1) return false;
      ++visits;
      continue;
    }
    if (live != 0 || visits != 2) return false;
    template for (int x : { S(live).get(), S(live).get() })
    {
      if (live != 1 || x != 1) return false;
      ++visits;
      break;
    }
    return live == 0 && visits == 3;
  }
}

namespace return_cleanup
{
  using transfer_cleanup::S;

  bool f(int& live)
  {
    template for (int x : { S(live).get(), S(live).get() })
    {
      return live == 1 && x == 1;
    }
  }

  bool g()
  {
    int live = 0;
    bool result = f(live);
    return result && live == 0;
  }
}

namespace nested_cleanup
{
  using transfer_cleanup::S;

  template<typename T> bool f()
  {
    int live = 0, visits = 0;
    template for (int x : { T(live).get(), T(live).get() })
    {
      if (live != 1 || x != 1) return false;
      template for (int y : { T(live).get(), T(live).get() })
      {
        if (live != 2 || y != 2) return false;
        ++visits;
        break;
      }
      if (live != 1) return false;
      continue;
    }
    return live == 0 && visits == 2;
  }
}

int main()
{
  return !transfer_cleanup::f() || !return_cleanup::g() ||
         !nested_cleanup::f<transfer_cleanup::S>();
}
