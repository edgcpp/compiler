typedef struct { long a, b, c; } Big3;
typedef struct { float x, y, z; } Hfa3;
Big3 make_big3(long s) { Big3 r = {s, s * 2, s * 3}; return r; }
Hfa3 scale_hfa3(Hfa3 in, float f) {
  Hfa3 r = {in.x * f, in.y * f, in.z * f}; return r;
}
