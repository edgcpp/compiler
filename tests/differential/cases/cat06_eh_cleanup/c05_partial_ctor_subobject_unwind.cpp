#include <cstdio>
    static int dtors = 0;
    struct SubA { SubA(){ dtors = dtors*10 + 1; }
                  ~SubA(){ dtors = dtors*10 + 8; } };
    struct SubB { SubB(){ dtors = dtors*10 + 2; }
                  ~SubB(){ dtors = dtors*10 + 7; } };
    struct Whole { SubA a; SubB b;
      Whole() { dtors = dtors*10 + 3; throw 55; }
      ~Whole() { dtors = dtors*10 + 9; } };
    int main() {
      try { Whole w; } catch (int e) { dtors = dtors*100 + e; }
      std::printf("%d\n", dtors);
      return 0;
    }
