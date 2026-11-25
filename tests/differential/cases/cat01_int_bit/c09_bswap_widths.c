#include <stdint.h>
    #include <stdio.h>
    int main(void) {
      uint16_t a = 0x1234U;
      uint32_t b = 0x12345678U;
      uint64_t c = 0x0123456789ABCDEFULL;
      printf("%x %x %llx\n", __builtin_bswap16(a), __builtin_bswap32(b),
             (unsigned long long)__builtin_bswap64(c));
      return 0;
    }
