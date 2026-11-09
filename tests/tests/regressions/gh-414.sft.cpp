//type:fp
//options:--c++20:--c++20 --g++ --gnu_version=150100:--c++20 --clang --clang_version=190000:--ms_c++20 --microsoft_version=1950

// A nontype template parameter of class type denotes a template parameter
// object, which is a const lvalue.  When such a parameter was used in a
// function template signature, substitution treated it as a prvalue: The
// decltype of the parenthesized name became the class type rather than a
// const reference to it (resulting in a spurious "unexpected function type"
// error), and taking its address caused substitution to fail.

template <typename T, typename U> constexpr bool is_same = false;
template <typename T> constexpr bool is_same<T, T> = true;

struct A { int i = 0; };
constexpr A a1{1}, a2{2};

template <A V> void test1(decltype((V)) v) { }
#if __GNUC__
template <A V> void test2(__decltype((V)) v) { }
#endif /* __GNUC__ */

void g() {
  constexpr A a;
  test1<a>(a);
#if __GNUC__
  test2<a>(a);
#endif /* __GNUC__ */
}

template <A V> auto paren() -> decltype((V));
static_assert(is_same<decltype(paren<a1>()), const A&>);

template <A V> auto no_paren() -> decltype(V);
static_assert(is_same<decltype(no_paren<a1>()), A>);

template <A V> auto member() -> decltype((V.i));
static_assert(is_same<decltype(member<a1>()), const int&>);

template <typename T, T V> void dependent(decltype((V))) { }
void h() { dependent<A, a1>(a1); }

template <A V> struct S { };
template <A V> auto deduced(S<V>) -> decltype((V));
static_assert(is_same<decltype(deduced(S<a1>{})), const A&>);

// The object designated in a rescanned signature is the same one designated
// in the instantiated function body.
template <A V> constexpr const A* addr() { return &V; }
template <A V> constexpr auto addr_in_sig() -> decltype(&V) { return &V; }
static_assert(addr<a1>() == addr_in_sig<a1>());
static_assert(addr_in_sig<a1>() != addr_in_sig<a2>());
static_assert(addr_in_sig<a2>()->i == 2);

template <A V> requires (&V != nullptr) constexpr int constrained() {
  return V.i;
}
static_assert(constrained<a2>() == 2);

template <typename T> struct X {
  template <A V> static auto f() -> decltype((V));
};
static_assert(is_same<decltype(X<int>::f<a1>()), const A&>);

template <A V> struct Y {
  template <typename T> static auto f(T) -> decltype(&V);
};
static_assert(is_same<decltype(Y<a1>::f(0)), const A*>);

// A closure-type argument that is itself the result of an earlier
// substitution (into the concept's template argument list).
template <auto F> concept Callable = (F() == 1);
template <auto F> constexpr int call() requires Callable<F> { return 1; }
template <auto F> constexpr int call() { return 0; }
static_assert(call<[]{ return 1; }>() == 1);
static_assert(call<[]{ return 2; }>() == 0);

// Template parameter objects with subaggregates (whose values must not be
// shared with the template argument value), designated in a template argument
// list in the signature.
namespace subaggregates {
  struct B { int x[3]; };
  struct C { B b; int y; };
  struct D { B bs[2]; };

  template <const B*> struct P { };
  template <B V> auto f() -> P<&V>;
  using R = decltype(f<B{{1, 2, 3}}>());

  template <const B* p> struct Q {
    static constexpr const B* ptr = p;
    static constexpr int value = p->x[2];
  };
  template <B V> constexpr auto g() -> Q<&V> { return {}; }
  template <B V> constexpr auto g2() -> Q<&V> { return {}; }
  template <B V> constexpr const B* addr() { return &V; }
  static_assert(decltype(g<B{{1, 2, 3}}>())::value == 3);
  static_assert(decltype(g<B{{1, 2, 3}}>())::ptr == addr<B{{1, 2, 3}}>());
  static_assert(is_same<decltype(g<B{{1, 2, 3}}>()),
                        decltype(g2<B{{1, 2, 3}}>())>);
  static_assert(decltype(g<B{{4, 5, 6}}>())::ptr !=
                decltype(g<B{{1, 2, 3}}>())::ptr);
  static_assert(g<B{{7, 8, 9}}>().value == 9);

  template <const C* p> struct QC { static constexpr int value = p->b.x[1]; };
  template <C V> auto h() -> QC<&V>;
  static_assert(decltype(h<C{{{1, 2, 3}}, 4}>())::value == 2);

  template <const D* p> struct QD {
    static constexpr int value = p->bs[1].x[0];
  };
  template <D V> auto k() -> QD<&V>;
  static_assert(decltype(k<D{{{{1, 2, 3}}, {{4, 5, 6}}}}>())::value == 4);
}  // namespace subaggregates
