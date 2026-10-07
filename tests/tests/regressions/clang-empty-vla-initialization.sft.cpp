//type:fn
//options_all:--c11 --clang_version 160000
//
// Clang versions before 17 do not accept empty initializers for variable-length
// arrays.

void f(int n) {
  int vla[n] = {};
}
