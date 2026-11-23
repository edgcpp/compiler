#include <cstdio>
    struct V { int v = 9; virtual ~V() = default; };
    struct A : virtual V { int a = 10; };
    struct B : virtual V { int b = 20; };
    struct Join : A, B { int j = 30; };
    int main() {
      Join j; V* vp = &j; A* ap = dynamic_cast<A*>(vp);
      B* bp = dynamic_cast<B*>(ap);
      std::printf("%d %d\n", ap ? ap->a : -1, bp ? bp->b : -1);
      return 0;
    }
