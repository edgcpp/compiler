#include <cstdint>
#include <cstdio>
struct alignas(64) Aligned64 {
  int payload[8];
};
struct Immovable {
  int x;
  int y;
  Immovable(int a, int b) : x(a), y(b) {}
  Immovable(const Immovable&) = delete;
  Immovable(Immovable&&) = delete;
};
static Immovable make_immovable(int a, int b) {
  return Immovable(a + 1, b + 2);
}
int main() {
  Aligned64* p = new Aligned64{};
  p->payload[0] = 55;
  bool ok_align = (reinterpret_cast<std::uintptr_t>(p) % 64U) == 0U;
  Immovable im = make_immovable(10, 20);
  int total = p->payload[0] + im.x + im.y + (ok_align ? 1 : 0);
  delete p;
  std::printf("%d\n", total);
  return 0;
}
