#include <stdint.h>
    #include <stdio.h>
    int main(void) {
      uint8_t u8 = 250U; u8 += 19U;
      uint16_t u16 = 65530U; u16 *= 37U;
      uint32_t u32 = 0xFFFFFFF0U; u32 = u32 * 2654435761U + 12345U;
      uint64_t u64 = 0xFFFFFFFFFFFFFF00ULL; u64 = u64 * 11400714819323198485ULL;
      printf("%u %u %u %llx\n", u8, u16, u32, (unsigned long long)u64);
      return 0;
    }
