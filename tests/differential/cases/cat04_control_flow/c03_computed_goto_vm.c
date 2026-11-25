#include <stdio.h>
    int run_vm(const int *ops) {
      static const void * const tbl[] = {&&L_ADD, &&L_MUL, &&L_SUB, &&L_HALT};
      int pc = 0, acc = 5;
      goto *tbl[ops[pc++]];
    L_ADD: acc += ops[pc++]; goto *tbl[ops[pc++]];
    L_MUL: acc *= ops[pc++]; goto *tbl[ops[pc++]];
    L_SUB: acc -= ops[pc++]; goto *tbl[ops[pc++]];
    L_HALT: return acc;
    }
    int main(void) {
      const int prog[] = {0, 7, 1, 3, 2, 10, 3};
      printf("%d\n", run_vm(prog));
      return 0;
    }
