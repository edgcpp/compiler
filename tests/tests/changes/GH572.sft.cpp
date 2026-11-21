//type:fp
//options:--c++11:--c++17
//remark:Braced casts to references to aggregates
//
// In generated C++, a braced functional-notation cast to a reference to an
// array, like R{1, 2} with "using R = int(&&)[2]", lost its type (becoming
// just "{1, 2}"), and one to a reference to a const array or to a class
// caused an internal error, as did a braced initializer for such a
// reference.  Such a cast to a reference to an array that was the constant
// initializer of a variable became a compound literal (a GNU extension).
// Also, a braced cast to an rvalue reference to an array was an lvalue
// rather than an xvalue.

struct S { int a, b; };
using RA = int(&&)[2];
using CA = const int(&)[2];
using CRA = const int(&&)[2];
using RS = S&&;
using CS = const S&;
using RSA = S(&&)[2];
template<class T> constexpr int size(T&&) { return sizeof(T); }
template<class T, class U> struct same { static const bool value = false; };
template<class T> struct same<T, T> { static const bool value = true; };
int i, j;

static_assert(size(RA{1, 2}) == 2 * sizeof(int), "");
static_assert(size(CA{1, 2}) == 2 * sizeof(int), "");
static_assert(size(CRA{1, 2}) == 2 * sizeof(int), "");
static_assert(size(RS{1, 2}) == sizeof(S), "");
static_assert(size(CS{1, 2}) == sizeof(S), "");
static_assert(size(RSA{{1, 2}, {3, 4}}) == 2 * sizeof(S), "");
static_assert(sizeof(decltype((RA{1, 2}))) == 2 * sizeof(int), "");
static_assert(same<decltype((RA{1, 2})), RA>::value, "");
static_assert(same<decltype((RA{i, j})), RA>::value, "");
static_assert(same<decltype((CA{1, 2})), CA>::value, "");
static_assert(same<decltype((CRA{1, 2})), CRA>::value, "");
static_assert(same<decltype((RS{1, 2})), RS>::value, "");
static_assert(same<decltype((RSA{{1, 2}, {3, 4}})), RSA>::value, "");
constexpr int h(const int (&)[2]) { return 1; }
constexpr int h(const int (&&)[2]) { return 2; }
static_assert(h(RA{1, 2}) == 2 && h(CA{1, 2}) == 1, "");
template<class T> constexpr bool is_lvalue(T&&) { return same<T, T&>::value; }
static_assert(!is_lvalue(RA{1, 2}) && is_lvalue(CA{1, 2}), "");
template<class T> constexpr bool cast_is_lvalue() {
  return is_lvalue(T{1, 2});
}
static_assert(!cast_is_lvalue<RA>() && cast_is_lvalue<CA>(), "");

const int (&r1)[2]{1, 2};
const S &r2{1, 2};
auto &&r3 = RA{1, 2};
CA r4 = CA{1, 2};
const S &r5 = RS{1, 2};
#if __cplusplus >= 201402L
decltype(auto) r6 = RA{1, 2};
decltype(auto) r7 = CA{1, 2};
decltype(auto) r8 = RS{1, 2};
static_assert(same<decltype(r6), RA>::value, "");
static_assert(same<decltype(r7), CA>::value, "");
static_assert(same<decltype(r8), RS>::value, "");
#endif
void g(const int (&)[2]);
void f() {
  auto &&a = RA{1, 2};
  auto &&b = CA{i, j};
  int n = size(RA{i, j}) + size(CS{i, j}) + size(RA{});
  g(CA{1, 2});
  g(RA{i, j});
  (void)a; (void)b; (void)n;
}
