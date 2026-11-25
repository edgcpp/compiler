#include <stdarg.h>
double weighted_var(int n, ...) {
  va_list ap; va_start(ap, n);
  double acc = 0.0;
  for (int i = 0; i < n; ++i) {
    int w = va_arg(ap, int);
    double x = va_arg(ap, double);
    acc += (double)w * x;
  }
  va_end(ap);
  return acc;
}
