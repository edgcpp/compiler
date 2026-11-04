//type: rp
//options: --c++26:--c++26 -tused
//remark:[GH #216] Enumerating expansion code generation and cleanup.

namespace element_order
{
  int f()
  {
    int n = 0, sum = 0;
    template for (auto x : { ++n, ++n, ++n })
    {
      if (x != n) return -1;
      sum = sum * 10 + x;
    }
    return sum;
  }
}

namespace distinct_statics
{
  int f()
  {
    int sum = 0;
    template for (auto x : { 1, 2 })
    {
      static int n = x;
      sum += n++;
    }
    return sum;
  }
}

namespace references_and_control_flow
{
  int f()
  {
    int a = 0, b = 0;
    template for (auto&& x : { a, b })
    {
      x = 1;
      continue;
    }
    template for (auto& x : { a, b })
    {
      ++x;
      break;
    }
    return a * 10 + b;
  }
}

namespace prototype_is_not_executed
{
  int calls = 0;
  int g() { return ++calls; }

  int f()
  {
    template for (constexpr auto x : { 1, 2 })
    {
      struct S { int n; };
      static S s = { g() };
      if (s.n != x) return -1;
    }
    template for (auto x : {})
    {
      static int n = g();
      (void)n;
    }
    return calls;
  }
}

namespace static_structured_bindings
{
  struct P { int a, b; };

  int f()
  {
    int sum = 0;
    template for (auto x : { 1, 2 })
    {
      static auto [a, b] = P{ x, x + 1 };
      sum += a + b;
    }
    return sum;
  }
}

namespace goto_out_of_expansion
{
  int f()
  {
    int sum = 0;
    template for (auto x : { 1, 2, 3 })
    {
      sum += x;
      if (x == 2) goto done;
    }
  done:
    return sum;
  }
}

int main()
{
  return element_order::f() != 123 || distinct_statics::f() != 3 ||
         distinct_statics::f() != 5 ||
         references_and_control_flow::f() != 21 ||
         prototype_is_not_executed::f() != 2 ||
         static_structured_bindings::f() != 8 ||
         static_structured_bindings::f() != 8 ||
         goto_out_of_expansion::f() != 3;
}
