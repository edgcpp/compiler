#include <cstdint>
    #include <cstdio>
    enum class Small : int8_t { Neg = -100, Zero = 0, Pos = 110 };
    enum class Wide : uint64_t { Big = 0xFEDCBA9876543210ULL };
    int main() {
      Small s = Small::Neg;
      Wide w = Wide::Big;
      std::printf("%d %llx\n", static_cast<int>(s),
                  static_cast<unsigned long long>(w));
      return 0;
    }
