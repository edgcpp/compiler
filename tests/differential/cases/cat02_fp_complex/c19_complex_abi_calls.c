/* DIFF_STD: -std=c11 */
#include <complex.h>
#include <stdio.h>
__attribute__((noinline))
double _Complex fma_cplx(double _Complex x, double _Complex y, double _Complex z) {
  return x * y + z;
}
__attribute__((noinline))
float _Complex scale_fc(float _Complex v, float k) {
  return v * k;
}
int main(void) {
  double _Complex r = fma_cplx(2.0 + 3.0 * I, -1.5 + 4.0 * I, 0.5 - 1.0 * I);
  float _Complex f = scale_fc(1.25f - 2.5f * I, 2.0f);
  printf("%.2f %.2f %.2f %.2f\n", creal(r), cimag(r), (double)crealf(f), (double)cimagf(f));
  return 0;
}
