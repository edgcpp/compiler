/* DIFF_STD: -std=c11 */
#include <stdatomic.h>
#include <stdio.h>
struct Pair { int x; int y; };
int main(void) {
  _Atomic int ai = 10;
  atomic_fetch_add(&ai, 5);
  _Atomic struct Pair ap;
  atomic_init(&ap, (struct Pair){3, 7});
  struct Pair cur = atomic_load(&ap);
  struct Pair nxt = {cur.x + 10, cur.y + 20};
  struct Pair old = atomic_exchange(&ap, nxt);
  struct Pair fin = atomic_load(&ap);
  printf("%d %d:%d %d:%d\n", atomic_load(&ai), old.x, old.y, fin.x, fin.y);
  return 0;
}
