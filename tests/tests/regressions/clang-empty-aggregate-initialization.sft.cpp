//type:fp
//options_all:--c11 --clang_version 30800
//
// Clang accepts empty initializers for aggregate types as an extension in
// pre-C23 C modes.

struct A {
  int i;
};

union U {
  int i;
  double d;
};

struct B {
  int i;
  struct A a;
};

struct A a = {};
union U u = {};
int fixed_array[4] = {};
int unknown_bound_array[] = {};
struct B nested = { 1, {} };
