//type:fp
//options_all:--c11 --clang_version 170000
//
// Beginning with Clang 17, empty initializers are accepted as an extension for
// scalar types and variable-length arrays as well as aggregates.

struct A {
  int i;
};

struct A aggregate = {};
int scalar = {};
double floating = {};
void *pointer = {};
int compound_literal = (int){};

void f(int n) {
  int vla[n] = {};
}
