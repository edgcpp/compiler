/* DIFF_STD: -std=c11 */
#include <stdio.h>
#include <stdlib.h>
#define KIND_TAG(x) _Generic((x), int: 10, double: 20, default: 30)
struct Packet {
  int count;
  int items[];
};
static int sum_vmp(int n, int (*mat)[n]) {
  return mat[0][n - 1] + mat[1][0];
}
int main(void) {
  int grid[2][4] = {[0] = {[3] = 15}, [1] = {[0] = 27}};
  struct Packet* p = (struct Packet*)malloc(sizeof(struct Packet) + 3 * sizeof(int));
  p->count = 3;
  int* src = (int[3]){4, 5, 6};
  for (int i = 0; i < 3; ++i) p->items[i] = src[i];
  int res = KIND_TAG(1) + KIND_TAG(2.5) + sum_vmp(4, grid)
            + p->items[0] + p->items[2];
  free(p);
  printf("%d\n", res);
  return 0;
}
