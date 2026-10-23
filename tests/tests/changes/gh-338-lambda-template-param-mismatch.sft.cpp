//type:fn
//options_all:--c++20 --clang --clang_version=210000
//remark:[GH #338] A lambda that names a template parameter: a wrong argument is still an error
//match_regex:line 24: error: no instance of function template "h" matches the argument list
//match_regex:line 25: error: no instance of function template "pick" matches the argument list
//match_regex:\A(?![\s\S]*line (?!2[45]\b)\d+: error)
// 10/16/26  [GH #338]
//
// See gh-338-lambda-template-param.  The template parameters are bound to the
// arguments of the instance for the rescan of the lambda: an argument that
// does not match what the lambda computes for those arguments is an error,
// only at the calls below.  (The other calls are valid.)  The template
// argument case is in gh-338-lambda-template-param-gnu.
template <class T> void h(char (*)[sizeof([](T y){ return y; }(T{}))]) {}
template <class T>
  __attribute__((enable_if(sizeof([]{ return T{}; }()) == sizeof(char), "")))
int pick(T) { return 1; }
void ok() {
  char a[sizeof(int)];
  h<int>(&a); pick('a');
}
void bad() {
  char a[sizeof(int)];
  h<char>(&a);
  pick(1);
}
