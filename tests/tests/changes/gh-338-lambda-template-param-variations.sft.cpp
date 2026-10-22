//type:fp
//options:--c++20:--c++23
//remark:[GH #338] A lambda that names a template parameter in an unnamed parameter or only in its body
// 10/16/26  [GH #338]
//
// See gh-338-lambda-template-param.  Variations from the review of the
// change: the lambda names T in the type of its unnamed parameter and in its
// body, or only in its body (its parameter has the type int).  Both are
// rescanned with T bound to the argument of each instance, and the calls are
// valid, as for Clang and GCC.  (These calls were accepted before the change
// too, although the rescan did not find T.  The calls with an array of the
// wrong size, which tell the two apart, and the generic variation are in
// gh-338-lambda-template-param-variations-negative.)

namespace named_in_param
{
  template<class T>
  void f(char (*)[sizeof([](T) { return T(); }(1))]) {}
  void g() { char a[sizeof(int)]; f<int>(&a); }
  void h() { char b[sizeof(long long)]; f<long long>(&b); }
}

namespace non_generic_lambda
{
  template<class T>
  void f(char (*)[sizeof([](int) { return T(); }(1))]) {}
  void g() { char a[sizeof(int)]; f<int>(&a); }
  void h() { char b[sizeof(long long)]; f<long long>(&b); }
}
