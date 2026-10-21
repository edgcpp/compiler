//type:fp
//options:--c++20 --gnu_version=999999:--c++20 --clang --clang_version=210000
//remark:[GH #338] The overload that is selected does not depend on a template parameter that has no value yet
// 10/16/26  [GH #338]
//
// See gh-338-lambda-template-param and gh-338-lambda-template-param-outer.
// Clang and GCC call the second h: the first one is not viable, because the
// bound of its array is 6 + sizeof(double) when the argument of U is deduced,
// and the argument is the array a that has 7 elements.  A rescan of the lambda
// that finds the global U (a struct that has the size 1) for the template
// parameter U, before the argument of U is deduced, makes the first h viable
// and better (it has no ellipsis), and selects it.
struct U { };
template <int N, class U>
char h(char (*)[([]{ return N + sizeof(U); }())], U) { return 0; }
template <int N>
long h(char (*)[7], ...) { return 0; }
char a[7];
static_assert(sizeof(h<6>(&a, 1.0)) == sizeof(long),
              "the second h must be selected");
