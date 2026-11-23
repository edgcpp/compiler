#include <compare>
    #include <cstdio>
    struct Key { int hi; int lo;
      auto operator<=>(const Key&) const = default; };
    int main() {
      Key a{1, 5}, b{1, 9}, c{1, 5};
      std::printf("%d %d %d\n", a < b, b > c, a == c);
      return 0;
    }
