#include <cstdio>
    struct Base { virtual ~Base() = default; int x = 1; };
    struct D1 : Base { int y = 42; };
    struct D2 : Base { int z = 99; };
    int main() {
      D1 d1; D2 d2; Base* b1 = &d1; Base* b2 = &d2;
      D1* ok = dynamic_cast<D1*>(b1);
      D1* no = dynamic_cast<D1*>(b2);
      std::printf("%d %d\n", ok ? ok->y : -1, no == nullptr);
      return 0;
    }
