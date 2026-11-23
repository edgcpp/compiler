#include <stdint.h>
    #include <stdio.h>
    int main(void) {
      int32_t v = 10;
      int32_t old1 = __atomic_fetch_add(&v, 5, __ATOMIC_SEQ_CST);
      int32_t new2 = __atomic_add_fetch(&v, 7, __ATOMIC_ACQ_REL);
      int32_t old3 = __atomic_exchange_n(&v, 100, __ATOMIC_SEQ_CST);
      int32_t exp = 100;
      int ok = __atomic_compare_exchange_n(&v, &exp, 250, 0,
                                           __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
      int32_t fin = __atomic_load_n(&v, __ATOMIC_ACQUIRE);
      printf("%d %d %d %d %d\n", old1, new2, old3, ok, fin);
      return 0;
    }
