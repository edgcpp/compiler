//type:fn
//options:--c++14:--c++20:--c++11 --g++ --gnu_version=90000:--c++20 --g++ --gnu_version=150000:--c++11 --clang_version 70000:--c++20 --clang_version 230100
// Subscripting an array prvalue or xvalue produces an xvalue (Core issue
// 1213), so the uses that need a modifiable lvalue are errors, as they are
// with GCC 9 and later and Clang 7 and later.  Previously "A().i[0]" was an
// lvalue and all of these were accepted.

struct A { int b; int i[2]; };
typedef int T2[2];
T2 a;

int *p1 = &A().i[0];                       // Error.
int *p2 = &static_cast<T2 &&>(a)[0];       // Error.
int &r1 = A().i[0];                        // Error.
int &r2 = 0[A().i];                        // Error.
void f() {
  A().i[0] = 1;                            // Error.
  ++T2{1, 2}[1];                           // Error.
}

// Lvalue arrays are not affected.
int *ok1 = &a[0];
int &ok2 = static_cast<T2 &>(a)[1];
