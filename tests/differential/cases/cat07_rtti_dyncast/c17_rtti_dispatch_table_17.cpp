#include <cstdio>
      #include <typeinfo>
      struct Root { virtual ~Root() = default; };
      struct KindA : Root { int a = 85; };
      struct KindB : Root { int b = 88; };
      struct KindAB : KindA, KindB {};
      int inspect(Root* p) {
        if (auto* a = dynamic_cast<KindA*>(p)) return a->a;
        if (auto* b = dynamic_cast<KindB*>(p)) return b->b;
        return -1;
      }
      int main() {
        KindA ka; KindB kb; KindAB kab;
        std::printf("%d %d %d\n", inspect(&ka), inspect(&kb),
                    dynamic_cast<KindB*>(static_cast<KindA*>(&kab))->b);
        return 0;
      }
