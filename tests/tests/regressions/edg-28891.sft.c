//type:rp
//options_all:--c --c11 --no_strict_gnu --gnu_version=130300
int main(void) {
  int k = 1, a[8], fails = 0;
  typeof (++k, (int (*)[k])a) p;
  (void)sizeof(p);
  fails += k != 2;
  // Each declarator evaluates the type specifier once.
  typeof (++k, (int (*)[k])a) q, r;
  (void)sizeof(q); (void)sizeof(r);
  fails += k != 4;
  typedef typeof (++k, (int (*)[k])a) T;
  fails += k != 5;
  T t;
  (void)sizeof(t);
  fails += k != 5; // A typedef use must not repeat its defining expression.
  typeof (++k, (int*)a) ordinary;
  (void)sizeof(ordinary);
  fails += k != 5; // A non-variably-modified operand is unevaluated.
  (void)(typeof (++k, (int (*)[k])a))a;
  fails += k != 6;
  (void)sizeof(typeof (++k, (int (*)[k])a));
  // sizeof a pointer does not require evaluating its variably modified type.
  // GNU C is permitted to omit that evaluation, so do not constrain k here.
  return fails;
}
