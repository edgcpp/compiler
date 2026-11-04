//type: fn
//options: --c++26 --no_warnings:--c++26 --no_warnings --microsoft_version 1950 --ms_permissive
//remark:[GH #216] Definition checking, element diagnostics, and recovery.

namespace definition_checking
{
  void f()
  {
    template for (auto x : {})
    {
      (void)unknown_name;
    }
  }
}

namespace element_checking
{
  void f()
  {
    template for (constexpr auto x : { 1, 2 })
    {
      static_assert(x == 1);
    }
  }
}

namespace constant_binding
{
  void f(int n)
  {
    template for (constexpr int x : { n })
    {
      (void)x;
    }
  }
}

namespace conflicting_declarations
{
  void f()
  {
    template for (int x = 0; auto x : {}) {}
    template for (auto x : { 1 })
    {
      int x;
    }
  }
}

namespace control_flow_boundary
{
  void f(int n)
  {
    switch (n)
    {
      template for (auto x : { 1 })
      {
        case 0: break;
      }
    }
    template for (auto x : { 1 })
    {
      label: (void)x;
    }
  }
}

namespace malformed_header
{
  void f()
  {
    template for (int x) {}
    template for (auto x : { , }) {}
  }
}

namespace restored_context
{
  void f()
  {
    template for (auto x : {}) { static_assert(false); }
    static_assert(false);
  }
}

namespace extern_storage_class
{
  void f()
  {
    template for (extern int x : { 1 })
    { (void)x; }
  }
}

namespace nonconstant_iterator_lambda_capture
{
  void f()
  {
    template for (auto i : { 1 })
    {
      [] () { return i; };
    }
  }
}

namespace goto_into_expansion
{
  void f()
  {
    goto in;
    template for (auto x : { 1 })
    {
    in:
      (void)x;
    }
  }
}

namespace undeclared_local_base
{
  void f()
  {
    template for (auto x : {0}) { struct S : Missing {}; }
  }
}

namespace constant_binding_break
{
  void f(int n)
  {
    template for (constexpr int x : { n, 2 })
    {
      break;
    }
  }
}
