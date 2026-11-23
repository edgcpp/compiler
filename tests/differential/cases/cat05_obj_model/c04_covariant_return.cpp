#include <cstdio>
    struct Left { int pad = 111; virtual ~Left() = default; };
    struct Base { int b = 22; virtual Base* self() { return this; }
                  virtual ~Base() = default; };
    struct Sub : Left, Base { int s = 33;
      Sub* self() override { return this; } };
    int main() {
      Sub x; Base* bp = &x; Base* r1 = bp->self();
      Sub* r2 = x.self();
      std::printf("%d %d %d\n", r1->b, r2->s, r2->pad);
      return 0;
    }
