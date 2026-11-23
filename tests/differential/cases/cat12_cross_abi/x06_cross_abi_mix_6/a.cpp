#include <cstdio>
      struct Payload { unsigned char data[4]; int tag; };
      Payload step_payload(Payload in, int delta);
      int risky_div(int a, int b);
      int main() {
        Payload p{}; p.tag = 3;
        for (int i = 0; i < 4; ++i) p.data[i] = static_cast<unsigned char>(i + 6);
        Payload q = step_payload(p, 6);
        int sum = q.tag;
        for (int i = 0; i < 4; ++i) sum += q.data[i];
        try { sum += risky_div(100, 0); } catch (int e) { sum += e; }
        std::printf("%d\n", sum);
        return 0;
      }
