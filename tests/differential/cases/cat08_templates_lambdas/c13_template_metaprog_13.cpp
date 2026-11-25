#include <cstdio>
      template <int N, int K> struct Binom {
        static constexpr int value = Binom<N-1, K-1>::value + Binom<N-1, K>::value;
      };
      template <int N> struct Binom<N, 0> { static constexpr int value = 1; };
      template <int N> struct Binom<N, N> { static constexpr int value = 1; };
      int main() {
        auto f = [bias = 13](auto v) { return v * 2 + bias; };
        std::printf("%d\n", f(Binom<14, 6>::value));
        return 0;
      }
