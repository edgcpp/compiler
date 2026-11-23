#include <cstdio>
      struct L0 { virtual int compute(int x) const { return x + 48; }
                   virtual ~L0() = default; };
      struct L1 : L0 { int compute(int x) const override {
        return L0::compute(x) * 2; } };
      struct L2 : L1 { int compute(int x) const override {
        return L1::compute(x) - 16; } };
      int main() {
        L0 a; L1 b; L2 c; const L0* arr[3] = {&a, &b, &c};
        int sum = 0; for (int i = 0; i < 3; ++i) sum += arr[i]->compute(i + 4);
        std::printf("%d\n", sum);
        return 0;
      }
