//type:fp
//options:--c++20 --gnu_version=999999:--c++20 --clang --clang_version=210000
//remark:[GH #338] A name that is spelled like a template parameter but is a qualified name or a member is not the template parameter
// 10/17/26  [GH #338]
//
// See gh-338-lambda-template-param and gh-338-lambda-template-param-outer.
// When a template argument is given explicitly (f<6>), the arguments of the
// other template parameters are not known yet while the declaration is
// substituted.  A lambda that spells the name of one of them (T below) after
// "::", "." or "->" does not name the template parameter: it is a qualified
// name or a member (::T, NS::T, g.T, and G::template T<int> and g.template
// T<int>, where the keyword "template" comes between), and the copy of the
// lambda must not fail because the parameter has no value.  The calls are
// valid, and the ones that have an overload that takes "..." select the first
// overload (the second is not viable for the argument), as for Clang and GCC.

// The global T, by a qualified name.
struct T { char c[3]; };
template <int N, class T> void f1(char (*)[sizeof([]{ return ::T{}; }())], T) {}
void use1() { char a[3]; f1<6>(&a, 1.0); }

// A member named T.
struct G { int T; };
template <int N, class T> void f2(char (*)[sizeof([]{ return G{}.T; }())], T) {}
void use2() { char a[sizeof(int)]; f2<6>(&a, 1.0); }

// A namespace member named U, by a qualified name.
namespace NS { struct U { char c[5]; }; }
template <int N, class U> void f3(char (*)[sizeof([]{ return NS::U{}; }())], U) {}
void use3() { char a[5]; f3<6>(&a, 1.0); }

// The overload that is selected.
template <int N, class T> char f4(char (*)[sizeof([]{ return ::T{}; }())], T) { return 0; }
template <int N> long f4(char (*)[3], ...) { return 0; }
char a4[3];
static_assert(sizeof(f4<6>(&a4, 1.0)) == sizeof(char),
              "the first f4 is viable and better");

// A member template named T, after "::template" and after ".template".
struct G1 { template <class> static constexpr int T = 3; };
template <int N, class T>
char f5(char (*)[([]{ return G1::template T<int>; }())], T) { return 0; }
template <int N> long f5(char (*)[3], ...) { return 0; }
char a5[3];
static_assert(sizeof(f5<6>(&a5, 1.0)) == sizeof(char),
              "the first f5 is viable and better");

struct G2 { template <class> static constexpr int T() { return 3; } };
template <int N, class T>
char f6(char (*)[([]{ return G2{}.template T<int>(); }())], T) { return 0; }
template <int N> long f6(char (*)[3], ...) { return 0; }
char a6[3];
static_assert(sizeof(f6<6>(&a6, 1.0)) == sizeof(char),
              "the first f6 is viable and better");
