//type:fp
//options:--c++20:--c++23
//remark:[GH #338] A lambda in a function template declaration that names a template parameter
// 10/16/26  [GH #338]
//
// A lambda in the declaration of a function template (in a parameter type, a
// default template argument, ...) is rescanned from its cached tokens when
// the template arguments are substituted for the template parameters.  The
// template parameters of most templates were not visible to the rescan: it
// did not find the names of the template parameters that the lambda uses,
// "identifier "T" is undefined", and the function template could not be
// called.  The template parameters are bound to the arguments for the rescan
// of the lambda now, and put back as they were afterwards.
//
// None of the declarations below is an error (as for Clang and GCC).  The
// cases that call the lambda in a template argument or in an array bound are
// in gh-338-lambda-template-param-gnu (they need --gnu_version).  A wrong
// argument is still an error (gh-338-lambda-template-param-mismatch), and a
// generic lambda is handled as before (gh-338-lambda-template-param-generic).

// A type parameter in an array bound, and in the parameter of the lambda.
// Three instances: each takes its own argument.
template <class T> void h(char (*)[sizeof([](T y){ return y; }(T{}))]) {}
void t2() {
  char a[sizeof(char)], b[sizeof(int)], c[sizeof(long)];
  h<char>(&a); h<int>(&b); h<long>(&c);
}

// A trailing return type: the parameter of the lambda and its body.
template <class T>
auto tr(T x) -> decltype([](T y){ return y; }(x)) { return x; }
int t4() { return tr(1) + tr(2); }

// A requires-clause and a default template argument.
template <class T>
  requires (sizeof([]{ return T{}; }()) == sizeof(int))
void rq(T) {}
template <class T, class U = decltype([]{ return T{}; }())>
U da() { return U{}; }
void t5() { rq(1); rq(2u); int v = da<int>(); char c = da<char>(); (void)v; (void)c; }

// The exception specification.
template <class T>
void ne(T) noexcept(noexcept([]{ return T{}; }())) {}
void t6() { ne(1); ne('a'); }

// The template parameters are put back when another instance of the same
// template is deduced while the first is: f<int> checks f<long> (found by
// argument-dependent lookup) in its constraint.
template <class T> struct W { };
template <class T>
  requires (sizeof([]{ static_assert(sizeof(T) >= 4); return T{}; }()) >= 4) &&
           (sizeof(T) > 4 || requires { f(W<decltype(T{} + 1L)>{}); })
int f(W<T>) { return sizeof(T); }
int t7() { return f(W<int>{}) + f(W<long>{}); }

// A lambda that calls a function template whose own declaration has a lambda
// that names its own template parameter.
template <class U>
auto gu(U u) -> decltype([](U v){ return v; }(u)) { return u; }
template <class T>
auto fu(T x) -> decltype([]{ return gu(T{}); }()) { return gu(x); }
int t8() { return fu(1) + fu(2); }

// A member template of a class template: the lambda names the template
// parameters of the class and of the member template.
template <class T> struct C {
  template <class U> void n(char (*)[sizeof([]{ return T{} + U{}; }())]) {}
  template <class U> void r(char (*)[sizeof([](U v){ return v; }(U{}))]) {}
};
void t9() {
  char a[sizeof(int)], b[sizeof(long)];
  C<int> c;
  c.n<short>(&a); c.n<long>(&b); c.r<int>(&a); c.r<long>(&b);
}

// A partial specialization of a class template that has a parameter pack: the
// lambda in its template argument names T.  (The pack is not bound.)
template <int N> struct B { };
template <class T, class... Args> struct S { static constexpr int v = 0; };
template <class T, class... Args>
struct S<T, B<sizeof([]{ return T{}; }())>, Args...> {
  static constexpr int v = 1;
};
static_assert(S<int, B<sizeof(int)>, long, char>::v == 1);
static_assert(S<int, B<sizeof(int) + 1>, long, char>::v == 0);
