#include <stdio.h>
int main(void) {
  long v = 21;
#if defined(__aarch64__)
  __asm__ volatile("add %0, %1, #21" : "=r"(v) : "r"(v));
#elif defined(__x86_64__)
  __asm__ volatile("addq $21, %0" : "+r"(v));
#else
  v += 21;
#endif
  printf("%ld\n", v);
  return 0;
}
