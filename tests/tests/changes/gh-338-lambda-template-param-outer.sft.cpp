//type:fn
//options:--c++20 --gnu_version=999999:--c++20 --clang --clang_version=210000
//remark:[GH #338] A template parameter that has no value is not looked up as an entity of the same name in an enclosing scope
//match_regex:line 25: error: no instance of function template "h" matches the argument list
//match_regex:\A(?![\s\S]*line (?!25\b)\d+: error)
// 10/16/26  [GH #338]
//
// See gh-338-lambda-template-param.  When a template argument is given
// explicitly (h<6>), the arguments of the other template parameters are not
// known yet while the declaration is substituted, and a lambda that names one
// of them (U, below) must not find the global U instead: the parameter that
// has no value makes the copy of the lambda fail, as it did before the lambdas
// could name the template parameters.  The bound of the array is 6 +
// sizeof(double) for h<6>(&a, 1.0); the array a has 6 + sizeof(::U) elements,
// so the call is an error, as for Clang and GCC (a rescan that binds the
// global U for U would accept it).  (Clang and GCC do not accept the call with
// the array that has 6 + sizeof(double) elements either: the lambda in the
// array bound is not a deduced context for them, and the substituted bound is
// a different expression; that is not what is pinned here.)
struct U { };
template <int N, class U>
void h(char (*)[([]{ return N + sizeof(U); }())], U) {}
void bad() {
  char a[6 + sizeof(::U)];
  h<6>(&a, 1.0);
}
