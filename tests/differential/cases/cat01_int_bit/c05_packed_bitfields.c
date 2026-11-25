#include <stdint.h>
    #include <stdio.h>
    struct __attribute__((packed)) PBF {
      uint8_t tag;
      uint32_t f1:13;
      int32_t f2:17;
      uint64_t f3:35;
    };
    int main(void) {
      struct PBF p = {7, 5001, -40000, 0x56789ABCDULL};
      p.f1 += 19; p.f2 += 1234; p.f3 ^= 0x111111111ULL;
      printf("%u %u %d %llu %zu\n", p.tag, p.f1, p.f2,
             (unsigned long long)p.f3, sizeof(struct PBF));
      return 0;
    }
