#include <stdint.h>
      #include <stdio.h>
      typedef int32_t v4i __attribute__((vector_size(16)));
      int main(void) {
        v4i x = {14, 28, 42, 56};
        v4i y = {4, 3, 2, 1};
        v4i z = x * y - x;
        z[2] += 17;
        printf("%d:%d:%d:%d\n", z[0], z[1], z[2], z[3]);
        return 0;
      }
