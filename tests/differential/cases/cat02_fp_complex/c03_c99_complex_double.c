#include <complex.h>
    #include <stdio.h>
    int main(void) {
      double _Complex a = 3.0 + 4.0 * I;
      double _Complex b = 1.0 - 2.0 * I;
      double _Complex s = a + b, p = a * b, q = a / b;
      printf("%.2f %.2f | %.2f %.2f | %.2f %.2f\n",
             creal(s), cimag(s), creal(p), cimag(p), creal(q), cimag(q));
      return 0;
    }
