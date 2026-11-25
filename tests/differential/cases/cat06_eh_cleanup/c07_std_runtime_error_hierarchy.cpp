#include <cstdio>
    #include <stdexcept>
    void fail(int mode) {
      if (mode == 1) throw std::out_of_range("oor");
      throw std::runtime_error("rt");
    }
    int main() {
      int score = 0;
      for (int m = 1; m <= 2; ++m) {
        try { fail(m); }
        catch (const std::logic_error&) { score += 10; }
        catch (const std::exception&) { score += 20; }
      }
      std::printf("%d\n", score);
      return 0;
    }
