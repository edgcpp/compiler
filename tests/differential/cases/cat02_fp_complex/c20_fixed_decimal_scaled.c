/* DIFF_STD: -std=c89 */
#include <stdio.h>
typedef long q16_16_t;
static q16_16_t q_mul(q16_16_t a, q16_16_t b) {
  long hi = (a >> 16) * (b >> 16);
  unsigned long alo = (unsigned long)(a & 0xFFFFL);
  unsigned long blo = (unsigned long)(b & 0xFFFFL);
  long mid = ((a >> 16) * (long)blo) + ((b >> 16) * (long)alo);
  unsigned long lo = (alo * blo) >> 16;
  return (hi << 16) + mid + (long)lo;
}
int main(void) {
  q16_16_t x = (3L << 16) + 0x8000L; /* 3.5 */
  q16_16_t y = (2L << 16) + 0x4000L; /* 2.25 */
  q16_16_t z = q_mul(x, y);          /* 7.875 == 7 * 65536 + 57344 = 516096 */
  printf("%ld\n", z);
  return 0;
}
