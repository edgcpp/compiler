//type:fp
//options:--c++11:--c++17
//remark:[GH #507] Braced cast to an rvalue reference that needs a temporary
//
// A braced functional cast to an rvalue reference type, like R{x} with
// "using R = int&&", produced an lvalue instead of an xvalue when the
// reference was bound to a temporary, i.e., when the element is not
// reference-related to the referenced type (a conversion function, a
// standard conversion, an empty list, or a list for an array).

template<class T> struct is_lref { static const bool value = false; };
template<class T> struct is_lref<T&> { static const bool value = true; };
template<class T> struct is_rref { static const bool value = false; };
template<class T> struct is_rref<T&&> { static const bool value = true; };
struct NE { operator int(); } ne;
struct NL { operator int&(); } nl;
using R = int&&;
using CR = const int&;
int &&a = R{ne};
static_assert(!is_lref<decltype((R{ne}))>::value, "");
static_assert(is_rref<decltype((R{ne}))>::value, "");
static_assert(is_rref<decltype((R(ne)))>::value, "");
static_assert(is_rref<decltype((R{1}))>::value, "");
static_assert(is_lref<decltype((CR{ne}))>::value, "");
static_assert(is_rref<decltype((R{nl}))>::value, "");
struct S { S(); S(int); };
struct CS { operator int(); } cs;
using RS = S&&;
S &&b = RS{cs};
static_assert(is_rref<decltype((RS{cs}))>::value, "");
int &&c = R{nl};
const int &d = CR{ne};
void f(int&&);
void g() { f(R{ne}); f(R{nl}); }

// Other cases where the reference is bound to a temporary.
using RL = long&&;
using CRR = const int&&;
using RA = int(&&)[2];
char ch;
int i;
static_assert(is_rref<decltype((R{'a'}))>::value, "");
static_assert(is_rref<decltype((R{ch}))>::value, "");
static_assert(is_rref<decltype((RL{i}))>::value, "");
static_assert(is_rref<decltype((R{}))>::value, "");
static_assert(is_rref<decltype((CRR{ne}))>::value, "");
template<class T> void t(int n) {
  static_assert(is_rref<decltype((T{n}))>::value, "");
  static_assert(is_rref<decltype((T{ch}))>::value, "");
}
template void t<long&&>(int);
int &&e = R{'a'};
long &&h = RL{i};
int (&&k)[2] = RA{1, 2};
