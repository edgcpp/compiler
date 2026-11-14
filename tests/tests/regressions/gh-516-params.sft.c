//type:rp
//options_all:--c --c11 --no_strict_gnu --gnu_version=130300
//remark:GNU C typeof operand in a block-scope prototype is not evaluated
// A typeof operand of variably modified type in a block-scope function
// prototype is not evaluated, and must not be inserted as a statement.
int a[8];
int main(void) {
  int m = 2;
  void local_proto(typeof (++m, (int (*)[m])a) p);
  void local_proto2(int n, typeof (++m, (int (*)[m])a) *p, int q[m]);
  return m != 2;
}
