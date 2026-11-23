// DIFF_FLAGS: -fopenmp
#include <cstdio>
int main() {
  long sum = 0;
  #pragma omp parallel for reduction(+:sum)
  for (int i = 1; i <= 1000; ++i) {
    sum += i;
  }
  std::printf("%ld\n", sum);
  return 0;
}
