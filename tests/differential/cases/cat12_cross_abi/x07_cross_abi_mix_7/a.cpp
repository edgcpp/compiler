#include <cstdio>
      struct Payload { unsigned char data[9]; int tag; };
      Payload step_payload(Payload in, int delta);
      int risky_div(int a, int b);
      int main() {
        Payload p{}; p.tag = 3;
        for (int i = 0; i < 9; ++i) p.data[i] = static_cast<unsigned char>(i + 7);
        Payload q = step_payload(p, 7);
        int sum = q.tag;
        for (int i = 0; i < 9; ++i) sum += q.data[i];
        try { sum += risky_div(100, 0); } catch (int e) { sum += e; }
        std::printf("%d\n", sum);
        return 0;
      }
