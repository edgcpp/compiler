#include <stdio.h>
    int vla_sum(int n) {
      int arr[n];
      for (int i = 0; i < n; ++i) arr[i] = (i + 1) * 3;
      int s = (int)(sizeof(arr) / sizeof(arr[0]));
      for (int i = 0; i < n; ++i) s += arr[i];
      return s;
    }
    int main(void) {
      int acc = 0;
      for (int k = 1; k <= 12; ++k) acc += vla_sum(k);
      printf("%d\n", acc);
      return 0;
    }
