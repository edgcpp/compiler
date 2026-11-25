#include <complex>
    #include <cstdio>
    int main() {
      std::complex<double> x(5.0, -12.0), y(3.0, 4.0);
      auto z = (x * y) + (x / y);
      std::printf("%.2f %.2f %.2f\n", z.real(), z.imag(), std::abs(x));
      return 0;
    }
