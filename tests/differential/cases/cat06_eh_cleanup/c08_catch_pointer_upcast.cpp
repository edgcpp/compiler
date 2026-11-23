#include <cstdio>
    struct B { int b = 14; virtual ~B() = default; };
    struct D : B { int d = 28; };
    int main() {
      D obj; int got = 0;
      try { D* p = &obj; throw p; }
      catch (const B* bp) { got = bp->b; }
      std::printf("%d\n", got);
      return 0;
    }
