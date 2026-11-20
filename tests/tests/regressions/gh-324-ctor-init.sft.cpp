//type:rp
//options:--c++14:--c++17:--c++20:--c++20 --g++ --gnu_version=150000:--c++20 --clang_version 230100:--c++23:--ms_c++20 --microsoft_version 1950
// An implicit "this" in a default member initializer that is used by a
// constructor refers to the object being constructed, even when the
// initializer aggregate-initializes another object of the same class.
// Each failing check prints its line number.
extern "C" int printf(const char *, ...);
int fails;
void check_(bool b, int line) {
  if (!b) {
    printf("check on line %d failed\n", line);
    ++fails;
  }
}
#define check(b) check_(b, __LINE__)

// MSVC leaves __cplusplus at 199711L; _MSVC_LANG gives the language version.
#ifdef _MSVC_LANG
#define LANG_VERSION _MSVC_LANG
#else
#define LANG_VERSION __cplusplus
#endif

struct Y { unsigned k = 7; unsigned n = Y{2, k}.n; };  // n = this->k

// Initializers whose value is known only at run time, so the initialization
// cannot be folded into a constant.
volatile unsigned seven = 7;
unsigned get_seven() { return seven; }
struct YV { unsigned k = seven; unsigned n = YV{2, k}.n; };
struct YF { unsigned k = get_seven(); unsigned n = YF{2, k}.n; };

// The constructor of Y is called by another constructor.
struct D : Y { D() {} };
struct D2 : Y {};
struct D3 : Y { D3() : Y() {} };
struct D4 : Y { D4() : Y{} {} };
struct H { Y y; H() {} };
struct H2 { Y y = Y(); };
struct HA { Y a[2]; HA() {} };

// Other forms of the default member initializer.
struct T1 { unsigned k = 7; unsigned n = T1{2, this->k}.n; };
struct T2 { unsigned k = 7; unsigned n = T2{2, T2{3, k}.n}.n; };
struct T3 {
  static T3 bar(T3 y) { return y; }
  unsigned k = 7;
  unsigned n = bar(T3{2, k}).n;
};
struct T4 {
  static unsigned get(const T4 &y) { return y.n; }
  unsigned k = 7;
  unsigned n = get(T4{2, k});
};
struct T5 { unsigned k = 7; unsigned n = [this] { return T5{2, k}.n; }(); };
#if LANG_VERSION > 201703L
struct T6 { unsigned k = 7; unsigned n = T6{.k = 2, .n = k}.n; };
#endif

#if LANG_VERSION >= 201703L
// An aggregate with a base class.
struct B { unsigned k = 7; };
struct Z : B { unsigned n = Z{{2}, k}.n; };
struct DZ : Z { DZ() {} };
#endif

template <class T> struct TY { T k = 7; T n = TY{2, k}.n; };
struct DT : TY<unsigned> { DT() {} };

// A user-provided constructor makes sure the default member initializers
// are used by a constructor (and not folded into a constant).
template <class T> struct Der : T { Der() {} };

// Nested aggregates of different classes, used by a constructor: the "this"
// of PA's default member initializer is the PA being initialized, not the
// enclosing PB or PC.
struct PA { unsigned x; unsigned y = x; };
struct PB { unsigned x = 42; PA a = PA{x}; };
struct PC { unsigned x = 42; PB b = PB{x}; PC() {} };

Y gy;
YV gyv;

int main() {
  Y y;
  check(y.n == 7);
  check(gy.n == 7);
  static Y sy;
  check(sy.n == 7);
  check((new Y)->n == 7);
  check((new Y())->n == 7);
  check(Y().n == 7);
  check(Y{}.n == 7);
  YV yv;
  check(yv.n == 7);             // Previously 2.
  check(gyv.n == 7);            // Previously 2.
  check((new YV)->n == 7);      // Previously 2.
  check(YV().n == 7);           // Previously 2.
  check(YV{}.n == 7);
  YF yf;
  check(yf.n == 7);             // Previously 2.
  check(D().n == 7);            // Previously 2.
  D2 d2;
  check(d2.n == 7);
  check(D3().n == 7);           // Previously 2.
  check(D4().n == 7);
  check(H().y.n == 7);          // Previously 2.
  H2 h2;
  check(h2.y.n == 7);
  check(HA().a[1].n == 7);      // Previously 2.
  Y arr[3];
  check(arr[2].n == 7);
  check((new Y[2])[1].n == 7);  // Previously 2.
  T1 t1;
  check(t1.n == 7);
  T2 t2;
  check(t2.n == 7);
  T3 t3;
  check(t3.n == 7);             // Previously 2.
  T4 t4;
  check(t4.n == 7);             // Previously 2.
  T5 t5;
  check(t5.n == 7);
  check(Der<T1>().n == 7);      // Previously 2.
  check(Der<T2>().n == 7);      // Previously 2.
  check(Der<T5>().n == 7);      // Previously 2.
#if LANG_VERSION > 201703L
  T6 t6;
  check(t6.n == 7);
  check(Der<T6>().n == 7);      // Previously 2.
#endif
#if LANG_VERSION >= 201703L
  check(DZ().n == 7);           // Previously 2.
#endif
  TY<unsigned> ty;
  check(ty.n == 7);
  check(TY<unsigned>().n == 7);
  check(DT().n == 7);           // Previously 2.
  check(Der<TY<int>>().n == 7); // Previously 2.
  Y a{1};
  check(a.n == 1);
  Y c = D();
  check(c.n == 7);              // Previously 2.
  PC pc;
  check(pc.b.a.y == 42);
  return fails;
}
