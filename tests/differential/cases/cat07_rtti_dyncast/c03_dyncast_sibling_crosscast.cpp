#include <cstdio>
    struct Left { int l = 15; virtual ~Left() = default; };
    struct Right { int r = 27; virtual ~Right() = default; };
    struct Both : Left, Right { int m = 3; };
    int main() {
      Both obj; Left* lp = &obj;
      Right* rp = dynamic_cast<Right*>(lp);
      void* vp = dynamic_cast<void*>(rp);
      std::printf("%d %d\n", rp ? rp->r : -1, vp == static_cast<void*>(&obj));
      return 0;
    }
