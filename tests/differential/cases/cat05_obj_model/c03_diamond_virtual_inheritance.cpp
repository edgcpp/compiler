#include <cstdio>
    static int order = 0;
    struct V { int v; V(int x):v(x){ order = order*10 + 1; }
      virtual int get() const { return v; }
      virtual ~V(){ order = order*10 + 9; } };
    struct L : virtual V { int l; L():V(99),l(10){ order = order*10 + 2; }
      ~L(){ order = order*10 + 8; } };
    struct R : virtual V { int r; R():V(88),r(20){ order = order*10 + 3; }
      ~R(){ order = order*10 + 7; } };
    struct D : L, R { D():V(42){ order = order*10 + 4; }
      int get() const override { return v + l + r; }
      ~D(){ order = order*10 + 6; } };
    int main() {
      int val = 0;
      { D d; V* pv = &d; val = pv->get(); }
      std::printf("%d:%d\n", val, order);
      return 0;
    }
