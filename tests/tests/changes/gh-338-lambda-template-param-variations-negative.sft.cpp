//type:fn
//options_all:--c++20
//remark:[GH #338] Variations of a lambda that names a template parameter: an argument of the wrong size, and a generic lambda
//match_regex:line 31: error: no instance of function template "named_in_param::f" matches the argument list
//match_regex:line 38: error: no instance of function template "non_generic_lambda::f" matches the argument list
//match_regex:line 44: error: identifier "T" is undefined
//match_regex:line 45: error: no instance of function template "generic_lambda::f" matches the argument list
//match_regex:\A(?![\s\S]*line (?!(31|38|44|45)\b)\d+: error)
// 10/16/26  [GH #338]
//
// See gh-338-lambda-template-param-variations: the variations from the review
// of the change, as negative cases.
//
// named_in_param and non_generic_lambda: the array does not have the size of
// int.  The bound of the parameter is computed by the lambda rescanned with T
// bound to int, so the call has no matching instance, as for Clang and GCC.
// (Before the change, "identifier "T" is undefined" was reported for the
// declarations as well: T was not found when the lambda was rescanned.)
//
// generic_lambda: the parameter of the lambda is auto, and T is named only in
// its body.  The template parameters are not bound yet for the rescan of a
// generic lambda (see bind_template_params_for_lambda_rescan in il.c): the
// body of its call operator is scanned when the operator is instantiated, in a
// context that does not include them.  So T is not found, and the call is an
// error.  (Clang and GCC accept it.)  This case records the limit; it moves to
// gh-338-lambda-template-param-variations when generic lambdas are handled.
namespace named_in_param
{
  template<class T>
  void f(char (*)[sizeof([](T) { return T(); }(1))]) {}
  void g() { char a[sizeof(int) + 1]; f<int>(&a); }
}

namespace non_generic_lambda
{
  template<class T>
  void f(char (*)[sizeof([](int) { return T(); }(1))]) {}
  void g() { char a[sizeof(int) + 1]; f<int>(&a); }
}

namespace generic_lambda
{
  template<class T>
  void f(char (*)[sizeof([](auto) { return T(); }(1))]) {}
  void g() { char a[sizeof(int)]; f<int>(&a); }
}
