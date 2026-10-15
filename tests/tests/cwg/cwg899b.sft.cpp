//type:fn
//options:--c++11:--c++11 --gnu_version 160200:--c++11 --clang_version 230100:--c++20:--c++20 --gnu_version 160200:--c++20 --clang_version 230100:--ms_c++20 --microsoft_version 1950
//options_all:-tused
//remark:Core issue 899: Explicit conversion functions in copy-initialization

struct C { };
struct A {
  explicit operator int() const;
  explicit operator C() const;
};

void f(A a) {
  int i = a;  // Error: copy-initialization.
  C c = a;    // Error: copy-initialization.
}
