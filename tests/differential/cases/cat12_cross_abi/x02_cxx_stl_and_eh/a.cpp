#include <cstdio>
    #include <stdexcept>
    #include <string>
    #include <vector>
    std::string build_greeting(int n);
    std::vector<int> build_seq(int n);
    int main() {
      std::string s = build_greeting(5);
      auto v = build_seq(4);
      int sum = 0; for (int x : v) sum += x;
      try { (void)build_greeting(-1); }
      catch (const std::exception& e) {
        std::printf("%s|%d|%s\n", s.c_str(), sum, e.what());
      }
      return 0;
    }
