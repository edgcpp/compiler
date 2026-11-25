#include <math.h>
    #include <stdio.h>
    int main(void) {
      double x = 16.0, y = -3.75;
      printf("%.2f %.2f %.2f %.2f %.2f\n",
             sqrt(x), fabs(y), floor(y), ceil(y), trunc(y));
      return 0;
    }
