#include <stdio.h>
    typedef struct { long a, b, c; } Big3;
    typedef struct { float x, y, z; } Hfa3;
    Big3 make_big3(long s);
    Hfa3 scale_hfa3(Hfa3 in, float f);
    int main(void) {
      Big3 b = make_big3(10);
      Hfa3 h = scale_hfa3((Hfa3){1.5f, 2.5f, 3.5f}, 2.0f);
      printf("%ld %.1f\n", b.a + b.b + b.c, (double)(h.x + h.y + h.z));
      return 0;
    }
