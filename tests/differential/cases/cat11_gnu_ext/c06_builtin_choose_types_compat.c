#include <stdint.h>
    #include <stdio.h>
    int main(void) {
      int a = __builtin_choose_expr(
          __builtin_types_compatible_p(int, int32_t), 111, 222);
      int b = __builtin_choose_expr(
          __builtin_types_compatible_p(int, double), 111, 222);
      long c = __builtin_expect(a + b, 333);
      printf("%ld\n", c);
      return 0;
    }
