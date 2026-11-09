//type:fp
//options_all:--c++20
//remark:Constexpr array new with a wide string literal initializer

// The number of elements of a string literal initializer is compared with the
// number of allocated elements, regardless of the character size.
constexpr int g(int n) {
  auto p = new wchar_t[n]{L"ab"};
  int r = p[1] + p[n - 1];
  delete[] p;
  return r;
}
static_assert(g(3) == L'b');
static_assert(g(4) == L'b');

constexpr int h(int n) {
  auto p16 = new char16_t[n]{u"ab"};
  auto p32 = new char32_t[n]{U"ab"};
  auto p8 = new char8_t[n]{u8"ab"};
  int r = (p16[1] == u'b') + (p32[1] == U'b') + (p8[1] == u8'b');
  delete[] p16;
  delete[] p32;
  delete[] p8;
  return r;
}
static_assert(h(3) == 3);
