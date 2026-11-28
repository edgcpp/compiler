//type:fp
//options_all:--c++14
//options::--g++ --gnu_version=150000:--clang_version 230100
template <class T> struct is_xvalue { static const bool value = false; };
template <class T> struct is_xvalue<T &&> { static const bool value = true; };
struct A { int i[2]; };
typedef int T2[2];
// Subscripting an array prvalue or xvalue produces an xvalue.
static_assert(is_xvalue<decltype((A().i[0]))>::value, "");
static_assert(is_xvalue<decltype((0[A().i]))>::value, "");
static_assert(is_xvalue<decltype((T2{1, 2}[0]))>::value, "");

//cwg: 1213
//title: Array subscripting and xvalues
//meeting: Bristol 4/13
//edg_status: Passes
