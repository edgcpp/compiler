/* DIFF_STD: -std=c11 */
#include <stdio.h>
struct RegBf {
  volatile unsigned a : 5;
  unsigned : 0;
  volatile signed int b : 7;
  unsigned long long strad : 20;
};
int main(void) {
  struct RegBf r = {19u, -23, 0xABCDEULL};
  r.a = (unsigned)(r.a + 3u);
  r.b = (signed int)(r.b - 5);
  printf("%u %d %llu %zu\n", (unsigned)r.a, (int)r.b,
         (unsigned long long)r.strad, sizeof(struct RegBf));
  return 0;
}
