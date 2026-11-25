#include <cstdio>
static int g_log = 0;
struct VRoot {
  int r = 10;
  virtual int tag() const { return 1; }
  VRoot() { g_log = g_log * 10 + tag(); }
  virtual ~VRoot() { g_log = g_log * 10 + tag(); }
};
struct Left : virtual VRoot {
  int l = 20;
  int tag() const override { return 2; }
  Left() { g_log = g_log * 10 + tag(); }
  ~Left() override { g_log = g_log * 10 + tag(); }
};
struct Right : virtual VRoot {
  int rg = 30;
  int tag() const override { return 3; }
  Right() { g_log = g_log * 10 + tag(); }
  ~Right() override { g_log = g_log * 10 + tag(); }
};
struct Join : Left, Right {
  int j = 40;
  int tag() const override { return 4; }
  Join() { g_log = g_log * 10 + tag(); }
  ~Join() override { g_log = g_log * 10 + tag(); }
};
int main() {
  VRoot* p = new Join();
  int Right::* pm_d = &Right::rg;
  int (VRoot::*pm_f)() const = &VRoot::tag;
  Join* jp = dynamic_cast<Join*>(p);
  int val = jp->*pm_d + (p->*pm_f)();
  delete p;
  std::printf("%d %d\n", val, g_log);
  return 0;
}
