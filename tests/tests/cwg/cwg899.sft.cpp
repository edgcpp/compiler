//type:fp
//options:--c++11:--c++11 --gnu_version 160200:--c++11 --clang_version 230100:--c++20:--c++20 --gnu_version 160200:--c++20 --clang_version 230100:--ms_c++20 --microsoft_version 1950
//options_all:-tused
//remark:Core issue 899: Explicit conversion functions in direct class initialization

struct C { };
struct A {
  explicit operator int() const;
  explicit operator C() const;
};

struct B {
  int i;
  B(const A& a): i(a) { }  // Okay.
};

void f(A a) {
  int j(a);   // Okay: direct-initialization.
  C c2(a);    // Okay: direct-initialization (Core issue 899).
}
