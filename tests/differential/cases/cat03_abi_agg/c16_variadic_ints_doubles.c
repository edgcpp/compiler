#include <stdarg.h>
    #include <stdio.h>
    __attribute__((noinline)) double vsum(int n, ...) {
      va_list ap, ap2;
      va_start(ap, n);
      va_copy(ap2, ap);
      double total = 0.0;
      for (int i = 0; i < n; ++i) {
        int iv = va_arg(ap, int);
        double dv = va_arg(ap, double);
        total += (double)iv + dv;
      }
      int first_i = va_arg(ap2, int);
      va_end(ap2);
      va_end(ap);
      return total + (double)first_i;
    }
    int main(void) {
      double r = vsum(6, 10, 0.25, 20, 0.5, 30, 0.75, 40, 1.0, 50, 1.25,
                      60, 1.5);
      printf("%.2f\n", r);
      return 0;
    }
