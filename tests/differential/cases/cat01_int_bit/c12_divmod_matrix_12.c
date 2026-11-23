#include <stdint.h>
      #include <stdio.h>
      int main(void) {
        int64_t acc_q = 0, acc_r = 0;
        for (int64_t i = -250; i <= 250; ++i) {
          int64_t n = i * 9700291LL + 204;
          int64_t dv = (i % 2 == 0) ? 1369LL : -1369LL;
          acc_q += n / dv;
          acc_r += n % dv;
        }
        printf("%lld:%lld\n", (long long)acc_q, (long long)acc_r);
        return 0;
      }
