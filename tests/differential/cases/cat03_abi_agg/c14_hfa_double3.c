#include <stdio.h>
    typedef struct { double x, y, z; } Vec3d;
    __attribute__((noinline)) Vec3d cross3(Vec3d a, Vec3d b) {
      Vec3d r = {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
                 a.x * b.y - a.y * b.x};
      return r;
    }
    int main(void) {
      Vec3d a = {2.0, 3.0, 4.0}, b = {5.0, 6.0, 7.0};
      Vec3d c = cross3(a, b);
      printf("%.1f %.1f %.1f\n", c.x, c.y, c.z);
      return 0;
    }
