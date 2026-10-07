#include "twice.h"
using fn = int (*)(int);
using move_fn = int &&(*)(int &);
fn other() { return &twice; }
move_fn other_move() { return &move_it<int>; }
