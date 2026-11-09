//type:fn
//options_all:--c++20
//remark:Array new with a too-long wide string literal initializer

constexpr int g(int n) {
  auto p = new wchar_t[n]{L"ab"};
  int r = p[1];
  delete[] p;
  return r;
}
static_assert(g(2) == L'b');  // Error: too few elements for the initializer.
