//type:fp
//options:--c++11:--c++14:--c++20:--c++14 --g++ --gnu_version=80000:--c++11 --g++ --gnu_version=90000:--c++20 --g++ --gnu_version=150000:--c++14 --clang_version 60000:--c++11 --clang_version 70000:--c++20 --clang_version 230100:--ms_c++20 --microsoft_version 1950
// Subscripting an array prvalue or xvalue produces an xvalue (Core issue
// 1213, a DR against C++11), and selecting a member of a class prvalue does
// too (Core issue 616, a C++14 change).  GCC adopted both in GCC 9, also in
// C++11 mode.  Clang adopted Core issue 616 in Clang 4 but Core issue 1213
// only in Clang 7, so Clang 4 to 6 give an xvalue for "A().b" but an lvalue
// for "A().i[0]".  MSVC (checked with 19.50) still gives an lvalue for the
// subscript.

template <class T> struct cat { static const int v = 0; };       // prvalue
template <class T> struct cat<T &> { static const int v = 1; };  // lvalue
template <class T> struct cat<T &&> { static const int v = 2; }; // xvalue
#define CAT(e) cat<decltype((e))>::v

#if defined(_MSC_VER)
#define SUB 1   // Category of a subscript of an array rvalue.
#elif defined(__clang__)
#define PRV (__clang_major__ >= 4 ? 2 : 0)  // Category of "prvalue.member".
#define SUB (__clang_major__ >= 7 ? 2 : 1)
#elif defined(__GNUC__)
#define PRV (__GNUC__ >= 9 ? 2 : 0)
#define SUB (__GNUC__ >= 9 ? 2 : 1)
#else
#define PRV (__cplusplus >= 201402L ? 2 : 0)
#define SUB (__cplusplus >= 201402L ? 2 : 1)
#endif

struct A { int b; int i[2]; int *p; };
struct S { int a[2]; };
typedef int T2[2];
A &la();
int arr[2];

#ifndef _MSC_VER
static_assert(CAT(A().b) == PRV, "A().b");
#endif
static_assert(CAT(A().i[0]) == SUB, "A().i[0]");
static_assert(CAT(0[A().i]) == SUB, "0[A().i]");
static_assert(CAT((T2{1, 2}[0])) == SUB, "T2{1, 2}[0]");
static_assert(CAT(0[S().a]) == SUB, "0[S().a]");
static_assert(CAT(static_cast<T2 &&>(arr)[0]) == SUB, "xvalue array");
#if !defined(_MSC_VER) && SUB == 2
// With both operands xvalues, the conditional expression is an xvalue.
static_assert(CAT(true ? A().b : A().i[0]) == 2, "?:");
#endif

// Lvalue arrays and pointers are not affected.
static_assert(CAT(la().i[0]) == 1, "la().i[0]");
static_assert(CAT(arr[0]) == 1, "arr[0]");
static_assert(CAT("ab"[0]) == 1, "\"ab\"[0]");
static_assert(CAT(A().p[0]) == 1, "A().p[0]");

template <class U> void t() {
  static_assert(CAT(U().i[0]) == SUB, "U().i[0]");
  static_assert(CAT(0[U().i]) == SUB, "0[U().i]");
}
template void t<A>();

// The subscript can still be used in a constant expression.
static_assert(T2{1, 2}[1] == 2, "T2{1, 2}[1]");
constexpr int v = S{{3, 4}}.a[1];
static_assert(v == 4, "S{{3, 4}}.a[1]");

#if SUB == 2
// An rvalue reference binds to the xvalue directly.  Previously an error.
int &&r1 = T2{1, 2}[0];
int &&r2 = S().a[0];
T2 a;
int &&r3 = static_cast<T2 &&>(a)[0];
#endif

#ifdef __GNUC__
// A static GNU compound literal designates a static object, so subscripting
// it still yields an lvalue (GCC and Clang accept this).
int &cl = (int[]){1, 2}[0];
#endif
