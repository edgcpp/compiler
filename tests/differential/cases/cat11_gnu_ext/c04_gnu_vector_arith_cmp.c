#include <stdint.h>
    #include <stdio.h>
    typedef int32_t v4i __attribute__((vector_size(16)));
    int main(void) {
      v4i a = {1, 20, 3, 40};
      v4i b = {5, 10, 3, 50};
      v4i s = a + b;
      v4i eq = (a == b);
      v4i lt = (a < b);
      printf("%d %d %d %d | %d %d %d %d | %d %d %d %d\n",
             s[0], s[1], s[2], s[3], eq[0], eq[1], eq[2], eq[3],
             lt[0], lt[1], lt[2], lt[3]);
      return 0;
    }
