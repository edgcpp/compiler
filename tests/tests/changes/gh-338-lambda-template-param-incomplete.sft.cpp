//type:fn
//options_all:--c++20 --gnu_version=999999
//remark:[GH #338] A lambda that names a template parameter is rescanned with the arguments that have a value only
//match_regex:line 25: error: more than one instance of overloaded function "g" matches the argument list
// 10/16/26  [GH #338]
//
// See gh-338-lambda-template-param.  The list of template arguments that a
// lambda is rescanned with can have arguments that have no value yet (a call
// of a function template whose template arguments are not all deduced, as of
// the two declarations of g below: the lambda is not a deduced context).  Only
// the template parameters that have a value are bound for the rescan, the
// others are not visible; binding all of them to the list made the rescan use
// a null constant for N and crash the front end (the case of
// imported/clang/cpp/SemaCXX/lambda-unevaluated).
//
// The two declarations are two function templates (their lambdas are two
// closure types), so the call is ambiguous, as Clang reports.  The errors
// "identifier "N" is undefined" at the declarations are not the point of this
// test: they are reported for the rescan of the declarations (they were before
// the change).
template <int N>
void g(const char (*)[([]{ return N; })()]);
template <int N>
void g(const char (*)[([]{ return N; })()]) {}
void use_g() { g<6>(&"hello"); }
