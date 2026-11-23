#include <cstdio>
    struct S { int a; int b; int c; };
    int read_field(const S& s, int S::* pm) { return pm ? s.*pm : -1; }
    int main() {
      S s{11, 22, 33};
      int S::* p1 = &S::b;
      int S::* pnull = nullptr;
      std::printf("%d %d %d\n", read_field(s, p1), read_field(s, pnull),
                  p1 != pnull);
      return 0;
    }
