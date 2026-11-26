//type:fn
//options_all: --c++11 --g++

// GNU C++ allows a variable-length array to be initialized, but array
// designators in such an initializer were lowered as if the designated
// array had a constant size: the designated values were silently lost,
// rows were written past the end of the array, or the compiler hit an
// internal error.  g++ does not support these either, so they are now
// diagnosed.  Designators for arrays of constant size within the
// initializer, field designators, and designators for arrays of pointers
// to variable-length arrays are still accepted.

struct S { int x, y; };
void use(void *);

void f(int n) {
  int a1[n] = {[1] = 7};
  int a2[3][n] = {[1] = {1, 2}};
  int a3[3][n] = {{[1] = 7}};
  int a4[n][3] = {[1] = {1, 2}};
  int a5[3][n] = {[2] = {[1] = 7}};
  int a6[n] = {[0 ... 1] = 4};
  int ok1[n][3] = {{[1] = 7}};
  S ok2[n] = {{.y = 1}};
  int (*ok3[2])[n] = {[1] = 0};
  int ok4[3][n] = {1, 2, 3, 4};
  use(a1); use(a2); use(a3); use(a4); use(a5); use(a6);
  use(ok1); use(ok2); use(ok3); use(ok4);
}
