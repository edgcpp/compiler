#include <stdint.h>
    #include <stdio.h>
    __attribute__((noinline)) double mix12(
        int a1, double d1, int a2, double d2, int a3, double d3,
        int a4, double d4, int a5, double d5, int a6, double d6,
        int a7, int a8, int a9, int a10, double d7, double d8,
        double d9, double d10) {
      long isum = a1 + a2*2 + a3*3 + a4*4 + a5*5 + a6*6 + a7*7 + a8*8
                  + a9*9 + a10*10;
      double fsum = d1 + d2*2 + d3*3 + d4*4 + d5*5 + d6*6 + d7*7 + d8*8
                    + d9*9 + d10*10;
      return (double)isum + fsum;
    }
    int main(void) {
      double r = mix12(1,0.5,2,1.5,3,2.5,4,3.5,5,4.5,6,5.5,7,8,9,10,
                       6.5,7.5,8.5,9.5);
      printf("%.2f\n", r);
      return 0;
    }
