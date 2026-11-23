#include <cstdio>
    struct Base { virtual int eval(int x) const { return x + 1; }
                  virtual ~Base() = default; };
    struct Derived : Base { int eval(int x) const override { return x * 3; } };
    int run(const Base& b, int v) { return b.eval(v); }
    int main() {
      Base b; Derived d;
      std::printf("%d %d\n", run(b, 5), run(d, 5));
      return 0;
    }
