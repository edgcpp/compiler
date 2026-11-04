//type: fn
//options: --c++26 --no_warnings
//remark: [GH #216] Invalid expansion element copy-initializations.

namespace nonliteral_zero
{
  constexpr int zero() { return 0; }
  constexpr int n = 0;
  void f()
  {
    template for (int *p : {0 + 0}) {}
    template for (int *p : {zero()}) {}
    template for (int *p : {n}) {}
    template for (int *p : {1}) {}
  }
}

namespace unresolved_overloads
{
  void f(int);
  void f(double);
  void g()
  {
    template for (auto p : {f}) {}
    template for (void (*p)(char) : {f}) {}
  }
}

namespace invalid_arrays
{
  void f()
  {
    char s[] = "a";
    template for (char a[2] : {s}) {}
    template for (char a[2] : {'a'}) {}
    template for (char a[1] : {"a"}) {}
    template for (char a[2] : {L"a"}) {}
    template for (int a[1] : {{1, 2}}) {}
    template for (int a[] : {{}}) {}
  }
}

namespace invalid_designated_initializers
{
  struct S { int a; int b; };

  void f()
  {
    template for (S s : {{.b = 2, .a = 1}}) {}
  }
}

namespace invalid_array_references
{
  void f()
  {
    template for (const int (&a)[1] : {{1, 2}}) {}
  }
}
