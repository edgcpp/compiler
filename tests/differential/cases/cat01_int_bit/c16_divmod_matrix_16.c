#include <stdint.h>
      #include <stdio.h>
      int main(void) {
        int64_t acc_q = 0, acc_r = 0;
        for (int64_t i = -250; i <= 250; ++i) {
          int64_t n = i * 12500375LL + 272;
          int64_t dv = (i % 2 == 0) ? 1493LL : -1493LL;
          acc_q += n / dv;
          acc_r += n % dv;
        }
        printf("%lld:%lld\n", (long long)acc_q, (long long)acc_r);
        return 0;
      }
