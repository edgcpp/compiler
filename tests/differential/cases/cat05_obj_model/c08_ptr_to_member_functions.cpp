#include <cstdio>
    struct Base { int k = 4;
      int nonvirt(int x) const { return x + k; }
      virtual int virt(int x) const { return x * 2; }
      virtual ~Base() = default; };
    struct Der : Base {
      int virt(int x) const override { return x * 5 + k; } };
    int invoke(const Base* b, int (Base::*pmf)(int) const, int v) {
      return (b->*pmf)(v);
    }
    int main() {
      Der d;
      std::printf("%d %d\n", invoke(&d, &Base::nonvirt, 10),
                  invoke(&d, &Base::virt, 10));
      return 0;
    }
