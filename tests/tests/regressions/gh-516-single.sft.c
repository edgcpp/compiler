//type:rp
//options_all:--c --c11 --no_strict_gnu --gnu_version=130300
//remark:GNU C typeof operand of variably modified type is evaluated once
// Each typeof operand of variably modified type is evaluated exactly once
// (with any back end): declarations with one declarator, a typedef, and a
// cast.
int main(void) {
  int k = 1, a[8], fails = 0;
  typeof (++k, (int (*)[k])a) p;
  (void)sizeof(p);
  fails += k != 2;
  typedef typeof (++k, (int (*)[k])a) T;
  T t;
  (void)sizeof(t);
  fails += k != 3;
  (void)(typeof (++k, (int (*)[k])a))a;
  fails += k != 4;
  typeof ((({ ++k; 0; }), (int (*)[k])a)) s;
  (void)sizeof(s);
  fails += k != 5;
  return fails;
}
