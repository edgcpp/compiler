#include <cstdio>
    static int dtor_trace = 0;
    struct Base { virtual ~Base() { dtor_trace = dtor_trace * 10 + 2; } };
    struct Child : Base { int x = 5;
      ~Child() override { dtor_trace = dtor_trace * 10 + 1; } };
    int main() {
      Base* p = new Child(); delete p;
      std::printf("%d\n", dtor_trace);
      return 0;
    }
