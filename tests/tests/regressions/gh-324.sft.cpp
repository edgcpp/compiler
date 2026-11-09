//type:rp
//options:--c++20:--c++20 --g++ --gnu_version=150000:--c++20 --clang
// An implicit "this" in a default member initializer refers to the object
// being initialized, even when the initializer contains an aggregate
// initialization of another object of the same class.
int fails;
void check(bool b) { if (!b) ++fails; }

struct Y { unsigned k; unsigned n = Y{2, k}.n; };  // n = this->k
struct Y2 {
  static Y2 bar(Y2 y) { return y; }
  unsigned k;
  unsigned n = bar(Y2{2, k}).n;
};
struct Y3 {
  static unsigned get(const Y3 &y) { return y.n; }
  unsigned k;
  unsigned n = get(Y3{2, k});
};
Y ret() { return Y{1}; }

// The default member initializer of the inner W still refers to the inner W.
struct W { int k; int j = k; int n = W{.k = 2, .n = 0}.j; };

// Anonymous unions and base classes.
struct U { int a; union { int b; int c; }; int d = U{7, {a}, 0}.b; };
struct B { int x; int y = x; };
struct D : B { int z = D{{3}, 0}.y + x; };

// "this" captured by a lambda in a default member initializer.
struct L { int k; int n = [this] { return L{2, k}.n; }(); };

// Initializers whose value is known only at run time, so the aggregate
// initialization cannot be folded into a constant.
volatile unsigned five = 5;
Y g{five};
Y ret_braced(unsigned v) { return {v}; }
unsigned take(Y y) { return y.n; }
struct C { Y y; C(unsigned v) : y{v} {} };
struct H { Y y; };
template <class T> struct TY { T k; T n = TY{2, k}.n; };

int main() {
  Y *p = new Y{1};
  check(p->n == 1);    // Previously 2.
  check(ret().n == 1); // Previously 2.
  Y y{1};
  check(y.n == 1);
  check((new Y2{1})->n == 1);  // Previously 2.
  check((new Y3{1})->n == 1);  // Previously 2.
#ifndef __clang__
  // In Clang mode, a member skipped by a designated initializer is not
  // initialized from its default member initializer (GH discussion #259).
  W *w = new W{5};
  check(w->j == 5 && w->n == 2);
#endif
  check((new U{1})->d == 1);
  D *d = new D{{5}};
  check(d->y == 5 && d->z == 8);
  check((new L{1})->n == 1);
  Y *a = new Y[2]{{1}, {6}};
  check(a[0].n == 1 && a[1].n == 6);
  unsigned v = five;
  Y yv{v};
  check(yv.n == 5);
  check(g.n == 5);
  check(ret_braced(v).n == 5);
  check(take({v}) == 5);
  check(C(v).y.n == 5);
  H h{{v}};
  check(h.y.n == 5);
  TY<unsigned> t{v};
  check(t.n == 5);
  return fails;
}
