//type:rp
//options:--c11:--c11 --gnu=150100:--c --gnu=40200
//source_files:inl.h use.c def.c
/* h, v, and k are used here and in use.c, but only def.c may define them.
   Each object used to contain an external definition of v, of h (with the
   GNU C89 inline rules), or of k (with the C99 inline rules). */
#include "inl.h"
int use(void);
int main(void) {
  return (h() == 1 && v(1, 2) == 1 && k() == 1 && use() == 3) ? 0 : 1;
}
