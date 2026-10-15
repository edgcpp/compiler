//type:fn
//options:--c++11:--c++11 --gnu_version 160200:--c++11 --clang_version 230100:--c++20:--c++20 --gnu_version 160200:--c++20 --clang_version 230100:--ms_c++20 --microsoft_version 1950
//options_all:-tused
//remark:Explicit conversions are not viable for braced reference arguments

int f(int&&);
int f(int*);
int g(const int&);
int g(int*);

struct A {
  explicit operator int() const;
};

int i = f({A{}});  // Error: cannot copy-list-initialize the temporary int.
int j = g({A{}});  // Likewise for a const lvalue reference.

struct B {
  operator int() const;
};

int k = f({B{}});  // Okay: implicit conversions are still considered.
int l = g({B{}});
