//type:rp
//options_all:--c++17 --g++ --exceptions

// Objects of an empty class.  When the generated C uses an empty struct for
// the class (size zero in GNU C), complete objects of the class must still
// get storage of the C++ size of the class (one byte), as code may write
// that many bytes (e.g., with memset or memcpy).  Before the fix, such
// objects had size zero in the generated C: distinct objects could have
// the same address, and writing to one could overwrite another object.
// This applies to variables of all storage durations, arrays of the class,
// by-value parameters (passed with size zero, as by g++), and address
// constants with an offset into an array of the class (e.g., "&ga[3]",
// which pointed to "ga").

typedef decltype(sizeof 0) size_t;
extern "C" void *memset(void *, int, size_t);
extern "C" void *memcpy(void *, const void *, size_t);
extern "C" int strcmp(const char *, const char *);

int errors;

#define CHECK(cond) do { if (!(cond)) ++errors; } while (0)

struct Z {};
struct Inc;                         /* Incomplete at its first use. */
extern Inc inc_early;
struct Inc {};
Inc inc_early;
Inc inc_late;

/* A nontrivial empty class. */
struct T {
  static int live;
  static const void *addrs[8];
  T() { addrs[live++ & 7] = this; }
  T(const T &) { addrs[live++ & 7] = this; }
  ~T() { live--; }
};
int T::live;
const void *T::addrs[8];

template <class X> struct Holder { static Z z; static Z za[3]; };
template <class X> Z Holder<X>::z;
template <class X> Z Holder<X>::za[3];
thread_local Z tl1, tl2;
const Z cz1{}, cz2{};
static volatile Z vz1, vz2;
Z ga[4];
Z *const pga = &ga[3];

static bool distinct(const void *a, const void *b) { return a != b; }

static long off(const void *p, const void *base) {
  return (const char *)p - (const char *)base;
}

/* Write the C++ size of an object. */
template <class X> static void scribble(X &x) { memset(&x, 0x5a, sizeof x); }

static int byval(Z a, Z b, int x) {
  int y = x;
  scribble(a);
  scribble(b);
  CHECK(y == x);
  return distinct(&a, &b);
}

static int unnamed(Z, int x, Z) { return x; }

template <class... P> static int pack(int x, P... p) {
  Z *ps[] = {&p...};
  for (unsigned k = 0; k < sizeof...(P); ++k) memset(ps[k], 0x5a, 1);
  CHECK(sizeof...(P) < 2 || ps[0] != ps[1]);
  return x;
}

struct K {
  int a;
  Z z;
  int b;
  K(Z q, int i) : a(i), z(q), b(i) {
    scribble(q);
    CHECK(a == i && b == i);
  }
};

struct B { virtual B *v(Z, int x, ...) { return this; } };
struct C0 { int pad; };
struct D : C0, B {
  D *v(Z z, int x, ...) {
    int y = x;
    scribble(z);
    CHECK(y == x);
    return this;
  }
};

static T make_t() { return T(); }

int main() {
  char g1[8] = "1234567";
  Z l1, l2;
  Z la[3];
  auto lam1 = [] { return 1; };
  auto lam2 = [] { return 2; };
  char g2[8] = "7654321";

  CHECK(distinct(&l1, &l2));
  CHECK(distinct(&la[0], &la[1]));
  CHECK(distinct(&lam1, &lam2));
  CHECK(distinct(&inc_early, &inc_late));
  CHECK(distinct(&Holder<int>::z, &Holder<long>::z));
  CHECK(off(&Holder<int>::za[2], &Holder<int>::za[0]) == 2);
  CHECK(distinct(&tl1, &tl2));
  CHECK(distinct(&cz1, &cz2));
  CHECK(distinct((const void *)&vz1, (const void *)&vz2));
  CHECK(off(&ga[3], &ga[0]) == 3);
  CHECK(off(pga, ga) == 3);
  CHECK(off(&la[2], &la[0]) == 2);
  CHECK(sizeof la == 3 && sizeof ga == 4);

  scribble(l1);
  scribble(l2);
  scribble(la);
  scribble(lam1);
  scribble(lam2);
  CHECK(lam1() == 1 && lam2() == 2);
  Z src;
  memcpy(&l1, &src, sizeof l1);
  l2 = src;
  la[1] = l1;
  CHECK(strcmp(g1, "1234567") == 0);
  CHECK(strcmp(g2, "7654321") == 0);

  /* By-value parameters. */
  CHECK(byval(l1, l2, 7));
  CHECK(unnamed(l1, 4, l2) == 4);
  CHECK(pack(5, l1, l2, l1) == 5);
  int (*fp)(Z, int) = [](Z a, int x) { int y = x; scribble(a); return y; };
  CHECK(fp(l1, 9) == 9);
  auto gl = [](auto a, int x) { int y = x; scribble(a); return y; };
  CHECK(gl(l1, 8) == 8);
  K k(l1, 6);
  CHECK(k.a == 6 && k.b == 6);
  D d;
  B *bp = &d;
  CHECK(bp->v(l1, 1, 2) == &d);

  /* Objects of a nontrivial empty class. */
  {
    T t1, t2;
    T ta[2];
    CHECK(T::live == 4);
    CHECK(T::addrs[0] == &t1 && T::addrs[1] == &t2);
    CHECK(T::addrs[2] == &ta[0] && T::addrs[3] == &ta[1]);
    T t3 = make_t();
    CHECK(T::live == 5);
    try {
      T t4;
      throw t4;
    } catch (T &c) {
      CHECK(distinct(&c, &t3));
    }
  }
  CHECK(T::live == 0);
  return errors != 0;
}
