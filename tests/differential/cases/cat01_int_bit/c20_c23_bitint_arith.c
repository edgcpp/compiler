/* DIFF_STD: -std=c23 */
#include <stdio.h>
int main(void) {
  _BitInt(7) a = -15;
  unsigned _BitInt(24) b = 100000u;
  _BitInt(40) prod = (_BitInt(40))a * (_BitInt(40))b;
  unsigned _BitInt(48) sh = ((unsigned _BitInt(48))b << 12) | 0x3FFu;
  printf("%lld %llu\n", (long long)prod, (unsigned long long)sh);
  return 0;
}
