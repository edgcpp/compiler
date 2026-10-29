//type:fn
//options:--c++20:--c++20 --gnu_version=150200:--c++20 --clang_version=210000
//remark:[GH #338] Diagnose invalid calls after substituting lambdas in template signatures.

namespace lambda_parameter_type_mismatch
{
  template<class T>
  void f(char (*)[sizeof([](T x) { return x; }(T{}))]) {}

  void test()
  {
    char a[sizeof(int)];
    f<int>(&a);
    f<char>(&a);
  }
}

namespace unnamed_lambda_parameter_mismatch
{
  template<class T>
  void f(char (*)[sizeof([](T) { return T(); }(1))]) {}

  void test()
  {
    char a[sizeof(int) + 1];
    f<int>(&a);
  }
}

namespace lambda_body_type_mismatch
{
  template<class T>
  void f(char (*)[sizeof([](int) { return T(); }(1))]) {}

  void test()
  {
    char a[sizeof(int) + 1];
    f<int>(&a);
  }
}

namespace indirect_template_parameter_dependency
{
  // Known limitation: T is used only indirectly through decltype(x), rather
  // than named in the lambda.  This valid call is still rejected.
  template<class T>
  void f(T x, char (*)[sizeof([](decltype(x) y) { return y; }(x))]) {}

  void test()
  {
    char a[sizeof(long long)];
    f(1LL, &a);
  }
}

#if defined(__GNUC__) || defined(__clang__)
namespace non_type_template_argument_mismatch
{
  template<int N> struct A {};
  template<int N> void f(A<[] { return N; }()>) {}

  void test()
  {
    f<1>(A<1>{});
    f<2>(A<2>{});
    f<1>(A<2>{});
  }
}

namespace nested_lambda_array_bound_mismatch
{
  template<int N, class T>
  void f(char (*)[([] { return [] { return N + int(sizeof(T)); }(); }())]) {}

  void test()
  {
    char a[1 + sizeof(int)], b[2 + sizeof(int)];
    f<1, int>(&a);
    f<1, int>(&b);
  }
}

namespace incomplete_template_arguments
{
  // The two lambdas have distinct closure types, so these declare distinct
  // templates.  Substitution with N not yet deduced must not crash.
  template<int N>
  void f(const char (*)[([] { return N; })()]);
  template<int N>
  void f(const char (*)[([] { return N; })()]) {}

  void test() { f<6>(&"hello"); }
}

namespace undeduced_parameter_hides_outer_name
{
  // U has no argument during the initial substitution of f<6>.  It must not
  // resolve to the enclosing struct U, which would incorrectly accept a.
  struct U {};
  template<int N, class U>
  void f(char (*)[([] { return N + sizeof(U); }())], U) {}

  void test()
  {
    char a[6 + sizeof(U)];
    f<6>(&a, 1.0);
  }
}
#endif

#ifdef __clang__
namespace enable_if_condition_not_satisfied
{
  template<class T>
    __attribute__((enable_if(sizeof([] { return T{}; }()) == sizeof(char), "")))
  int pick(T) { return 1; }

  void test()
  {
    pick('a');
    pick(1);
  }
}
#endif
