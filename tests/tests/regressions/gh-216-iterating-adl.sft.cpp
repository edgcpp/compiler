//type: fn
//options: --c++26
//remark:[GH #216] Member and ADL bounds select the iterating form.

namespace enum_bounds
{
  enum E { e };

  void begin(E);
  void end(E);

  void f(E v)
  {
    template for (auto x : v) { (void)x; }
  }
}

namespace pointer_bounds
{
  struct X { };

  void begin(X *);
  void end(X *);

  void f(X *p)
  {
    template for (auto x : p) { (void)x; }
  }
}

namespace rvalue_only_bounds
{
  struct S { int value; };

  void begin(S&&);
  void end(S&&);

  void f()
  {
    template for (auto x : S{1}) { (void)x; }
  }

  void g(S &s)
  {
    template for (auto x : static_cast<S&&>(s)) { (void)x; }
  }
}

namespace member_bounds
{
  struct I { };

  struct S
  {
    int value;
    I begin();
    I end();
  };

  void f(S s)
  {
    template for (auto x : s) { (void)x; }
  }
}

namespace uninstantiated_member_bounds
{
  template<typename T> struct S
  {
    T value;
    const T* begin() const { return &value; }
    const T* end() const { return &value + 1; }
  };

  /* Finding the members must instantiate S<int>; the reference parameter
     does not already require a complete type. */
  void f(const S<int>& s)
  {
    template for (auto x : s) { (void)x; }
  }
}
