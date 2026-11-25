#include <stdint.h>
    #include <stdio.h>
    int main(void) {
      uint64_t x = 0xF0F0ULL;
      __atomic_fetch_or(&x, 0x0F00ULL, __ATOMIC_SEQ_CST);
      __atomic_fetch_and(&x, 0xFF33ULL, __ATOMIC_SEQ_CST);
      __atomic_fetch_xor(&x, 0x1111ULL, __ATOMIC_SEQ_CST);
      uint64_t r = __atomic_sub_fetch(&x, 0x10ULL, __ATOMIC_SEQ_CST);
      printf("%llx\n", (unsigned long long)r);
      return 0;
    }
