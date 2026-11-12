//type:fp
//options:--c++14:--c++20
//remark:Repeated constexpr initialization of a local array from a string

// Initializing a local array from a string literal must not leave its storage
// associated with the string literal: The same storage is initialized again
// in the next iteration of a loop.

constexpr int f() {
  int r = 0;
  for (int i = 0; i < 2; ++i) {
    char a[2] = "a";
    r += a[0];
  }
  return r;
}
static_assert(f() == 2*'a', "");

constexpr int g() {
  int r = 0;
  for (int i = 0; i < 3; ++i) {
    char a[4] = "abc";
    a[i] = 'x';
    r = r*256 + a[0];
  }
  return r;
}
static_assert(g() == ('x'*256 + 'a')*256 + 'a', "");

constexpr long h() {
  long r = 0;
  for (int i = 0; i < 2; ++i) {
    const char a[] = "ab";
    char16_t b[3] = u"cd";
    wchar_t c[4] = L"ef";
    r += a[1] + b[1] + c[1];
  }
  return r;
}
static_assert(h() == 2*('b' + u'd' + L'f'), "");
