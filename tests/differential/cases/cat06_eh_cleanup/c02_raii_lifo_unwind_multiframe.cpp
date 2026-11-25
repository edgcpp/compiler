#include <cstdio>
    static int log_seq = 0;
    struct Scoped { int id; Scoped(int i):id(i){ log_seq = log_seq*10 + id; }
      ~Scoped(){ log_seq = log_seq*10 + (id + 4); } };
    void inner() { Scoped s3(3); throw 99; }
    void middle() { Scoped s2(2); inner(); }
    int main() {
      try { Scoped s1(1); middle(); }
      catch (int v) { log_seq = log_seq * 100 + v; }
      std::printf("%d\n", log_seq);
      return 0;
    }
