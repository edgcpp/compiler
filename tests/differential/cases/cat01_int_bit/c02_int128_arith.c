#include <stdint.h>
    #include <stdio.h>
    int main(void) {
      unsigned __int128 u = ((unsigned __int128)0x123456789ABCDEF0ULL << 64)
                            | 0xFEDCBA9876543210ULL;
      unsigned __int128 q = u / 97U;
      unsigned __int128 r = u % 97U;
      __int128 s = -(__int128)(u >> 5);
      __int128 sq = s / -53;
      printf("%llx:%llx:%llu:%llx\n",
             (unsigned long long)(q >> 64), (unsigned long long)q,
             (unsigned long long)r, (unsigned long long)sq);
      return 0;
    }
