#include <cstdio>
    #include <typeinfo>
    struct Base { virtual ~Base() = default; };
    struct D1 : Base { int v = 7; };
    struct D2 : Base { int w = 8; };
    int main() {
      D1 d1; Base& br = d1; int res = 0;
      res += dynamic_cast<D1&>(br).v;
      try { (void)dynamic_cast<D2&>(br); }
      catch (const std::bad_cast&) { res += 100; }
      std::printf("%d\n", res);
      return 0;
    }
