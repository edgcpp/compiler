//type:rp
//options_all:--c++11 --g++
//options:--exceptions:--no_exceptions

// Brace initialization of GNU variable-length arrays of a class type with a
// nontrivial destructor.  When exceptions are disabled, such an
// initialization is lowered by keeping the constant part of the initializer
// and leaving the zeroing of the remainder to the back end; that did not
// zero the array at all when no constant part was kept ("= {}"), and zeroing
// gives the wrong value for a null pointer to data member, which is not
// all-bits-zero in the IA-64 ABI.

int errors;

static void check(int got, int expected) {
  if (got != expected) ++errors;
}

// Leave nonzero garbage on the stack so that missing zeroing is detected.
static void scribble() {
  volatile int junk[256];
  for (int i = 0; i < 256; i++) junk[i] = 0x5a5a5a5a;
}

struct S { int a, b; };
struct DO { int v; ~DO() {} };
struct M { int S::*p; int v; ~M() {} };
struct W { int k; M m; };

static int empty_init(int n) {
  DO a[n] = {};
  DO b[2][n] = {};
  DO c[n][2] = {};
  DO d[n]{};
  int sum = 0;
  for (int i = 0; i < n; i++) {
    sum += a[i].v + d[i].v + c[i][0].v + c[i][1].v;
    sum += b[0][i].v + b[1][i].v;
  }
  return sum;
}

static void ptr_to_member(int n) {
  M a[n] = {{&S::b, 5}};
  M b[n][2] = {{{&S::b, 5}}};
  M c[2][n] = {{{&S::b, 5}}};
  W w[n] = {{1, {&S::b, 5}}};
  M e[n + 1] = {{&S::b, 5}, {nullptr, 6}};
  for (int i = 0; i < n; i++) {
    check(a[i].p == (i == 0 ? &S::b : nullptr), 1);
    check(a[i].v, i == 0 ? 5 : 0);
    for (int j = 0; j < 2; j++) {
      check(b[i][j].p == (i == 0 && j == 0 ? &S::b : nullptr), 1);
      check(b[i][j].v, i == 0 && j == 0 ? 5 : 0);
      check(c[j][i].p == (i == 0 && j == 0 ? &S::b : nullptr), 1);
      check(c[j][i].v, i == 0 && j == 0 ? 5 : 0);
    }
    check(w[i].k, i == 0 ? 1 : 0);
    check(w[i].m.p == (i == 0 ? &S::b : nullptr), 1);
    check(w[i].m.v, i == 0 ? 5 : 0);
  }
  check(e[0].p == &S::b, 1);
  for (int i = 1; i <= n; i++) {
    check(e[i].p == nullptr, 1);
    check(e[i].v, i == 1 ? 6 : 0);
  }
}

int main() {
  for (int n = 1; n <= 4; n++) {
    scribble();
    check(empty_init(n), 0);
    scribble();
    ptr_to_member(n);
  }
  return errors != 0;
}
