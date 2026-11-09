//type:rp
//remark:A break in a GNU statement expression in a switch condition
//options_all:--g++ --c++20

constexpr int f(int y, bool c) {
  switch (y) {
  case 0:
    switch (({ if (c) break; 1; })) {
    case 1:
      return 10;
    }
    return 1;
  }
  return 2;
}
static_assert(f(0, false) == 10);
static_assert(f(0, true) == 2);
static_assert(f(5, true) == 2);

constexpr int g(bool c) {
  int r = 0;
  for (int i = 0; i < 3; ++i) {
    switch (({ if (c) break; i; })) {
    default:
      r += 1;
    }
    r += 100;
  }
  return r;
}
static_assert(g(false) == 303);
static_assert(g(true) == 0);

constexpr int h(int c) {
  int r = 0;
  do {
    switch (({ int v = 0;
               switch (c) {
               case 1:
                 v = 5;
                 break;
               }
               v; })) {
    case 5:
      r = 50;
      break;
    default:
      r = 1;
    }
    r += 2;
  } while (false);
  return r;
}
static_assert(h(1) == 52);
static_assert(h(0) == 3);

int main() {
  volatile bool t = true, f0 = false;
  volatile int zero = 0, one = 1, five = 5;
  return !(f(zero, f0) == 10 && f(zero, t) == 2 && f(five, t) == 2 &&
           g(f0) == 303 && g(t) == 0 && h(one) == 52 && h(zero) == 3);
}
