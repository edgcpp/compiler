//type:fp
//options_all:--c++14
//options:--g++ --gnu_version=80300:--clang --clang_version=30900
// GCC before 9 and Clang before 4 do not implement Core issue 616: selecting
// a member of a class prvalue still produces a prvalue (the C++11 rule).
template <class T> struct is_prvalue { static const bool value = true; };
template <class T> struct is_prvalue<T &> { static const bool value = false; };
template <class T> struct is_prvalue<T &&> { static const bool value = false; };
struct A { int b; };
static_assert(is_prvalue<decltype((A().b))>::value, "");
