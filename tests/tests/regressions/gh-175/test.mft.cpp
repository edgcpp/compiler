//type:rp
//options:--c++17 --g++:--c++17 --g++ -tused:--c++17 --clang
//source_files:twice.h other.C
#include "twice.h"
using fn = int (*)(int);
using move_fn = int &&(*)(int &);
fn other();
move_fn other_move();
int main() {
  int i = 1;
  return (&twice == other() && twice(1) == 2 &&
          &move_it<int> == other_move() && move_it(i) == 1) ? 0 : 1;
}
