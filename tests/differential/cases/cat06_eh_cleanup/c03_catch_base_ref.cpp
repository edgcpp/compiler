#include <cstdio>
    struct ErrBase { int code; ErrBase(int c):code(c){}
      virtual int val() const { return code; } virtual ~ErrBase()=default; };
    struct ErrSub : ErrBase { ErrSub(int c):ErrBase(c){}
      int val() const override { return code + 100; } };
    int main() {
      int got = 0;
      try { throw ErrSub(23); }
      catch (const ErrBase& e) { got = e.val(); }
      std::printf("%d\n", got);
      return 0;
    }
