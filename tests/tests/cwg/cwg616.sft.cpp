//type:fp
//options:--c++14:--c++14 --g++ --gnu_version=150000:--c++14 --clang --clang_version=190100:--c++11 --g++ --gnu_version=90000:--c++11 --clang --clang_version=40000
template <class T> struct is_xvalue { static const bool value = false; };
template <class T> struct is_xvalue<T &&> { static const bool value = true; };
struct A { int b; };
// Selecting a member of a class prvalue produces an xvalue.
static_assert(is_xvalue<decltype((A().b))>::value, "");
static_assert(is_xvalue<decltype((A().*&A::b))>::value, "");

//cwg: 616
//title: Definition of "indeterminate value"
//meeting: Bristol 4/13
//edg_status: Passes
