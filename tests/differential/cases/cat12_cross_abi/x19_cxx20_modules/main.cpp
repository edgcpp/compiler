import m_math;
#include <cstdio>
int main() {
  ModBox b{9};
  std::printf("%d %d\n", triple_add(10, 7), b.scaled(4));
  return 0;
}
