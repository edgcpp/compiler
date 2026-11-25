#include <cstdio>
    struct A { int a = 10; virtual int fa() { return a; } virtual ~A()=default; };
    struct B { int b = 25; virtual int fb() { return b; } virtual ~B()=default; };
    struct C : A, B {
      int c = 7;
      int fa() override { return a + c; }
      int fb() override { return b + c * 2; }
    };
    int call_b(B* p) { return p->fb(); }
    int main() {
      C obj; A* pa = &obj; B* pb = &obj;
      std::printf("%d %d\n", pa->fa(), call_b(pb));
      return 0;
    }
