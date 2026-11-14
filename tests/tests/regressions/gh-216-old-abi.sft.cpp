//type: cp
//options: --c++26
//requires:IA64_ABI=1
//remark:[GH #216] Structured-binding containers in older ABI configurations.

struct P { int a, b; };

int f(bool c)
{
  if (c) {
    static auto [a, b] = P{1, 2};
    return a + b;
  } else {
    static auto [a, b] = P{3, 4};
    return a + b;
  }  /* if */
}
