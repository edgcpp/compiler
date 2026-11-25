/* DIFF_STD: -std=c11 */
#include <stdarg.h>
#include <stdio.h>
typedef struct { int a; long b; } SmallS;
typedef struct { long x, y, z, w; } BigS;
typedef struct { float x, y, z; } HfaF3;
typedef struct { double u, v; } HfaD2;
__attribute__((noinline))
long sum_va(int tag, ...) {
  va_list ap;
  va_start(ap, tag);
  SmallS s = va_arg(ap, SmallS);
  HfaF3 hf = va_arg(ap, HfaF3);
  BigS bg = va_arg(ap, BigS);
  HfaD2 hd = va_arg(ap, HfaD2);
  va_end(ap);
  return (long)tag + s.a + s.b + (long)(hf.x + hf.y + hf.z)
         + bg.x + bg.w + (long)(hd.u * 10.0 + hd.v * 10.0);
}
int main(void) {
  SmallS s = {11, 22};
  HfaF3 hf = {1.5f, 2.5f, 3.0f};
  BigS bg = {100, 200, 300, 400};
  HfaD2 hd = {1.25, 2.75};
  printf("%ld\n", sum_va(5, s, hf, bg, hd));
  return 0;
}
