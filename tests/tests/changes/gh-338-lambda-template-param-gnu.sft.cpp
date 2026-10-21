//type:fn
//options_all:--c++20 --gnu_version=999999
//remark:[GH #338] A lambda called in a template argument or an array bound of a function template declaration that names a template parameter
//match_regex:line 32: error: no instance of function template "g" matches the argument list
//match_regex:line 33: error: no instance of function template "k" matches the argument list
//match_regex:\A(?![\s\S]*line (?!3[23]\b)\d+: error)
// 10/16/26  [GH #338]
//
// See gh-338-lambda-template-param.  The lambda in the declarations below is
// called in a constant expression: a template argument (the case of
// imported/gnu/cpp/cpp2a/lambda-uneval7) and an array bound.  Its template
// parameters are bound to the arguments of the instance for the rescan, so the
// two instances of each template have their own parameter types, and an
// argument of the type of the other instance is an error, at the two calls at
// the end only.  (The other calls are valid, as for Clang and GCC.)
//
// Without --gnu_version the front end still reports, for the valid calls,
// "template instantiation resulted in unexpected function type": the call of
// the new lambda is not folded to a constant in the type that is built to
// check the instance.  That is not changed here.
template <int N> struct A { };
template <int N> void g(A<[]{return N;}()>) {}
template <int N, class T>
void k(char (*)[([]{ return [] { return N + int(sizeof(T)); }(); }())]) {}
void ok() {
  char a[1 + sizeof(int)], b[1 + sizeof(long)], c[4 + sizeof(long)];
  g<1>({}); g<2>({}); g<1>(A<1>{}); g<2>(A<2>{});
  k<1, int>(&a); k<1, long>(&b); k<4, long>(&c);
}
void bad() {
  char b[100];
  g<1>(A<2>{});
  k<1, int>(&b);
}
