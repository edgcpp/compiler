//type:rp
//options_all:--c++11 --g++

// Row indexing and pointer arithmetic on multidimensional arrays of an
// empty class.  When the generated C uses an empty struct for the class
// (size zero in GNU C), pointers to its rows also point to a type of size
// zero in the generated C, so pointer arithmetic on them must be scaled by
// the size of the row.  Before the fix, this was done only for pointers to
// the class itself: the rows of "Z a[3][2]" all had the same address, and
// the difference of two row pointers did not compile.  Rows that are GNU
// variable-length arrays are scaled by their run-time size.

int errors;

struct Z {};
static Z *addrs[16];
static int nctors;
struct C {
  C() { addrs[nctors++] = (Z *)this; }
};

#define CHECK(cond) do { if (!(cond)) ++errors; } while (0)

static long off(const void *p, const void *base) {
  return (const char *)p - (const char *)base;
}

static void vla(int n) {
  Z a[3][n];
  Z (*p)[n] = a;
  Z (*q)[n] = p + 1;
  CHECK(off(&a[1][0], a) == n);
  CHECK(off(&a[2][1], a) == 2 * n + 1);
  CHECK(off(a[2], a) == 2 * n);
  CHECK(off(q, a) == n);
  CHECK(off(&q[1], a) == 2 * n);
  CHECK(q - p == 1);
  CHECK(&a[2] - p == 2);
  CHECK(off(++p, a) == n);
  CHECK(off(p++, a) == n);
  CHECK(off(p, a) == 2 * n);
  CHECK(off(p--, a) == 2 * n);
  CHECK(off(--p, a) == 0);
  p += 2;
  CHECK(off(p, a) == 2 * n);
  p -= 1;
  CHECK(off(p, a) == n);

  Z b[2][n][3];
  Z (*r)[n][3] = b;
  CHECK(off(&b[1][1][2], b) == 3 * n + 5);
  CHECK(off(b[1][1], b) == 3 * n + 3);
  CHECK(off(r + 1, b) == 3 * n);
  CHECK((r + 1) - r == 1);
  Z (*s)[3] = b[1];
  CHECK(off(s + 1, b) == 3 * n + 3);

  nctors = 0;
  C c[2][n];
  CHECK(nctors == 2 * n);
  for (int k = 0; k < 2 * n; ++k) CHECK(off(addrs[k], c) == k);
}

volatile int two = 2, three = 3;

int main() {
  vla(two);
  vla(three);
  Z a[3][2];
  Z (*p)[2] = a;
  Z (*q)[2] = p + 1;
  CHECK(off(&a[1][0], a) == 2);
  CHECK(off(&a[2][1], a) == 5);
  CHECK(off(a[2], a) == 4);
  CHECK(off(q, a) == 2);
  CHECK(off(&q[1], a) == 4);
  CHECK(off(&1[q], a) == 4);
  CHECK(off(q - 1, a) == 0);
  CHECK(q - p == 1);
  CHECK(&a[2] - p == 2);
  CHECK(off(++p, a) == 2);
  CHECK(off(p++, a) == 2);
  CHECK(off(p, a) == 4);
  CHECK(off(p--, a) == 4);
  CHECK(off(--p, a) == 0);
  p += 2;
  CHECK(off(p, a) == 4);
  p -= 1;
  CHECK(off(p, a) == 2);
  int i = 2;
  CHECK(off(p + i - 2, a) == 2);
  CHECK(off(&a[i][1], a) == 5);

  Z b[2][2][3];
  Z (*r)[2][3] = b;
  CHECK(off(&b[1][1][2], b) == 11);
  CHECK(off(b[1][1], b) == 9);
  CHECK(off(r + 1, b) == 6);
  CHECK((r + 1) - r == 1);
  Z (*s)[3] = b[1];
  CHECK(off(s + 1, b) == 9);
  CHECK(s + 1 - b[0] == 3);

  /* Each element of a 2-D array of a class constructed at its address. */
  nctors = 0;
  C c[3][2] = {};
  for (int k = 0; k < 6; ++k) CHECK(off(addrs[k], c) == k);
  return errors != 0;
}
