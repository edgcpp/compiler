//type:rp
//require:DO_IL_LOWERING 1
//options_all:--c --c11 --no_strict_gnu --gnu_version=170000
//remark:GNU C typeof operand of variably modified type is evaluated

// With a source-generating back end, the typeof is reproduced and the run-time
// behavior is that of the compiler of the generated code.  GCC versions before
// 17 evaluate the operand once per declarator (GCC PR c/124576), so a recent
// version is emulated here.

int k, a[8], fails;
#define T(e) typeof (e, (int (*)[k])a)
#define EXPECT(v) (fails += k != (v))

static int va_arg_case(int x, ...) {
  __builtin_va_list ap;
  __builtin_va_start(ap, x);
  void *p = __builtin_va_arg(ap, T(++k));
  __builtin_va_end(ap);
  return p == a;
}

int main(void) {
  int n = 2;
  k = 1;
  T(++k) p;
  (void)sizeof(p);
  EXPECT(2);
  // The type specifier is evaluated once per declaration, not once per
  // declarator.
  k = 1;
  T(++k) q, r;
  (void)sizeof(q); (void)sizeof(r);
  EXPECT(2);
  // A use of a typedef does not repeat the evaluation.
  k = 1;
  typedef T(++k) TT;
  EXPECT(2);
  TT t;
  (void)sizeof(t);
  EXPECT(2);
  // An operand that is not variably modified is not evaluated.
  k = 1;
  typeof (++k, (int *)a) ordinary;
  (void)sizeof(ordinary);
  EXPECT(1);
  k = 1;
  (void)(T(++k))a;
  EXPECT(2);
  // A statement expression nested in the operand is evaluated once too.
  k = 1;
  typeof ((({ ++k; 0; }), (int (*)[k])a)) s, u;
  (void)sizeof(s); (void)sizeof(u);
  EXPECT(2);
  // Compound literals, used as an rvalue and as an lvalue.
  k = 1;
  (void)(T(++k)){0};
  EXPECT(2);
  k = 1;
  void *addr = &(T(++k)){0};
  (void)addr;
  EXPECT(2);
  k = 1;
  fails += !va_arg_case(0, (int (*)[2])a);
  EXPECT(2);
  // sizeof applied to a variable-length array type evaluates its operand.
  k = 1;
  (void)sizeof(T(++k)[n]);
  EXPECT(2);
  return fails;
}
