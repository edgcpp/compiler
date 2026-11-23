#include <math.h>
    #include <stdio.h>
    int main(void) {
      volatile double z = 0.0;
      double pinf = 1.0 / z, ninf = -1.0 / z, nanv = z / z;
      double nzero = copysign(0.0, -1.0);
      printf("%d %d %d %d %d %d\n", isinf(pinf) != 0, isinf(ninf) != 0,
             isnan(nanv) != 0, signbit(nzero) != 0, !(nanv == nanv),
             nanv != nanv);
      return 0;
    }
