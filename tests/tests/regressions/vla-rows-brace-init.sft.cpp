//type:rp
//options_all:--c++11 --g++
//options:--exceptions:--no_exceptions

// Brace initialization of GNU variable-length arrays whose rows have
// variable length (e.g., "int a[2][n] = {{1}, {2, 3}};").  The initializer
// can be neither kept as a C initializer nor copied from a static temporary
// (the layout of the rows is known only at run time), so the elements are
// assigned individually after the whole array has been zeroed.  Elements
// without an explicit initializer must be zero (or, for a class with a
// default member initializer or constexpr constructor, default-initialized).

int errors;

static void check(int got, int expected) {
  if (got != expected) ++errors;
}

struct P { int x, y; };
struct CD { int v; constexpr CD() : v(-1) {} ~CD() {} };
struct W { CD c; int i = 3; };
struct DO { int v; ~DO() {} };
struct S { int a, b; };
struct Q { int S::*p; int v; };

static void empty_rows(int n) {
  int a[2][n] = {};
  for (int i = 0; i < 2; i++)
    for (int j = 0; j < n; j++) check(a[i][j], 0);
}

static void partial_rows(int n) {
  int a[3][n] = {{1}, {2, 3}, {}};
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < n; j++)
      check(a[i][j], i == 0 && j == 0 ? 1 :
                     i == 1 && j == 0 ? 2 :
                     i == 1 && j == 1 ? 3 : 0);
}

static void middle_variable_dim(int n) {
  int a[2][n][2] = {{{1, 2}, {3}}, {{4}}};
  for (int i = 0; i < 2; i++)
    for (int j = 0; j < n; j++)
      for (int k = 0; k < 2; k++)
        check(a[i][j][k], i == 0 && j == 0 ? k + 1 :
                          i == 0 && j == 1 && k == 0 ? 3 :
                          i == 1 && j == 0 && k == 0 ? 4 : 0);
}

static void inner_variable_dim(int n) {
  int a[2][2][n] = {{{1}, {2, 3}}, {{}, {4}}};
  for (int i = 0; i < 2; i++)
    for (int j = 0; j < 2; j++)
      for (int k = 0; k < n; k++)
        check(a[i][j][k], i == 0 && j == 0 && k == 0 ? 1 :
                          i == 0 && j == 1 && k == 0 ? 2 :
                          i == 0 && j == 1 && k == 1 ? 3 :
                          i == 1 && j == 1 && k == 0 ? 4 : 0);
}

static void all_variable_dims(int n) {
  // The initializer has the fixed type int[2][2] here.
  int a[n][n] = {{1}, {2, 3}};
  for (int i = 0; i < n; i++)
    for (int j = 0; j < n; j++)
      check(a[i][j], i == 0 && j == 0 ? 1 :
                     i == 1 && j == 0 ? 2 :
                     i == 1 && j == 1 ? 3 : 0);
}

static void typedef_rows(int n) {
  typedef int R[n];
  R a[2] = {{1}, {2, 3}};
  for (int i = 0; i < 2; i++)
    for (int j = 0; j < n; j++)
      check(a[i][j], i == 0 && j == 0 ? 1 :
                     i == 1 && j == 0 ? 2 :
                     i == 1 && j == 1 ? 3 : 0);
}

static void nonconstant_element(int n) {
  int x = n * 10;
  int a[2][n] = {{x, 1}, {2}};
  for (int i = 0; i < 2; i++)
    for (int j = 0; j < n; j++)
      check(a[i][j], i == 0 && j == 0 ? x :
                     i == 0 && j == 1 ? 1 :
                     i == 1 && j == 0 ? 2 : 0);
}

static void string_rows(int n) {
  char a[2][n] = {"ab", "c"};
  for (int i = 0; i < 2; i++)
    for (int j = 0; j < n; j++)
      check(a[i][j], i == 0 && j == 0 ? 'a' :
                     i == 0 && j == 1 ? 'b' :
                     i == 1 && j == 0 ? 'c' : 0);
}

static void struct_rows(int n) {
  P a[2][n] = {{{1, 2}}, {{3}}};
  for (int i = 0; i < 2; i++)
    for (int j = 0; j < n; j++) {
      check(a[i][j].x, i == 0 && j == 0 ? 1 : i == 1 && j == 0 ? 3 : 0);
      check(a[i][j].y, i == 0 && j == 0 ? 2 : 0);
    }
}

static void class_rows(int n) {
  W w[2][n] = {};
  CD c[2][n] = {};
  DO d[2][n] = {{{5}}, {{6}, {7}}};
  for (int i = 0; i < 2; i++)
    for (int j = 0; j < n; j++) {
      check(w[i][j].i, 3);
      check(w[i][j].c.v, -1);
      check(c[i][j].v, -1);
      check(d[i][j].v, i == 0 && j == 0 ? 5 :
                       i == 1 && j == 0 ? 6 :
                       i == 1 && j == 1 ? 7 : 0);
    }
}

static void ptr_to_member_rows(int n) {
  // A null pointer to data member is not all-bits-zero in the IA-64 ABI.
  Q a[2][n] = {{{&S::b, 5}}};
  for (int i = 0; i < 2; i++)
    for (int j = 0; j < n; j++) {
      check(a[i][j].p == (i == 0 && j == 0 ? &S::b : nullptr), 1);
      check(a[i][j].v, i == 0 && j == 0 ? 5 : 0);
    }
}

int main() {
  for (int n = 3; n <= 4; n++) {
    empty_rows(n);
    partial_rows(n);
    middle_variable_dim(n);
    inner_variable_dim(n);
    all_variable_dims(n);
    typedef_rows(n);
    nonconstant_element(n);
    string_rows(n);
    struct_rows(n);
    class_rows(n);
    ptr_to_member_rows(n);
  }
  return errors != 0;
}
