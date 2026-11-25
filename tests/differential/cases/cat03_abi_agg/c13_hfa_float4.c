#include <stdio.h>
    typedef struct { float x, y, z, w; } Vec4f;
    __attribute__((noinline)) Vec4f lerp4(Vec4f a, Vec4f b, float t) {
      Vec4f r = {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
                 a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t};
      return r;
    }
    int main(void) {
      Vec4f u = {1.0f, 2.0f, 3.0f, 4.0f}, v = {5.0f, 10.0f, 15.0f, 20.0f};
      Vec4f w = lerp4(u, v, 0.25f);
      printf("%.2f %.2f %.2f %.2f\n", (double)w.x, (double)w.y,
             (double)w.z, (double)w.w);
      return 0;
    }
