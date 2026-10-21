//type:fn
//options_all:--c++20
//remark:[GH #338] A generic lambda in a function template declaration that names a template parameter is not rescanned with the arguments
//match_regex:line 16: error: identifier "T" is undefined
//match_regex:line 17: error: no instance of function template "f" matches the argument list
//match_regex:\A(?![\s\S]*line (?!1[67]\b)\d+: error)
// 10/16/26  [GH #338]
//
// See gh-338-lambda-template-param.  Only a lambda that is not generic and has
// no template parameter list of its own has the template parameters of the
// function template bound to the arguments for the rescan.  A generic lambda
// is rescanned as before, so the name of the template parameter is still not
// found here, as it was before the change.  (Clang and GCC accept the
// declaration.)  This case records the limit of the change.
template <class T>
void f(char (*)[sizeof([](auto x){ return T(x); }(1))]) {}
void use() { char a[sizeof(int)]; f<int>(&a); }
