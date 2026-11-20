//type:fp
//options:--c++23
//remark:Casts to unnamed closure types and closure typedefs in generated C++
//
// In generated C++, a cast to a closure type for which no typedef had been
// put out, like auto{[](int a) { return a; }} or auto{m} with m declared in a
// for-init, was put out with the unnamed closure type written as if it were
// a class name.  Also, a typedef for the closure type of a variable declared
// in a condition or range-based for statement was put out inside the
// declaration.

int i = 3;
template<class T> constexpr int size(T&&) { return sizeof(T); }
struct C { C(); C(const C&); int v; };
auto g() { return [](int a) { return a; }; }

static_assert(auto{[](int a) { return a; }}(2) == 2);
static_assert(auto{auto{[] { return 1; }}}() == 1);
static_assert(size(auto{[] {}}) > 0);
int x = auto{[](int a) { return a + i; }}(2);
int y = sizeof(auto{[] {}}) + size(auto{[] {}});
auto h1 = auto{[] { return 1; }};
int z = auto{g()}(2) + auto(g())(3);

int f1() {
  int j = 1;
  return auto{[&] { return i + j; }}() +
         auto{[j]() mutable { return ++j; }}() +
         auto{[](auto a) { return a; }}(j) +
         auto{[c = C()] { return c.v; }}();
}

template<class T> T f2() { return auto{[] { return T(1); }}(); }
int w = f2<int>();

int f3() {
  int n = 0;
  for (auto m = [n = 0]() mutable { return ++n; }; auto{m}() < 3; auto(m)()) {
    n += m();
  }
  for (const auto c = [k = 0]() mutable { return ++k; }; n < 9; ++n) {
    n += auto{c}();
  }
  for (auto m = [c = C()] { return c.v; }; n < 12; ++n) {
    n += auto{m}() + auto(m)();
  }
  for (auto m = g(); n < 15; ++n) {
    n += auto{m}(1);
  }
  return n;
}

int f4() {
  return [m = [n = 0]() mutable { return ++n; }]() mutable {
    return auto{m}() + m();
  }();
}

struct R {
  struct It {
    int n;
    auto operator*() { return g(); }
    It& operator++() { ++n; return *this; }
    bool operator!=(It o) const { return n != o.n; }
  };
  It begin() { return {0}; }
  It end() { return {2}; }
};

int f5() {
  int n = 0;
  if (auto m = [] { return 1; }) {
    n += m() + auto{m}();
  }
  switch (auto m = [] { return 2; }; m()) {
    default:
      n += m();
  }
  while (auto m = [] { return 3; }) {
    n += m();
    break;
  }
  for (auto m : R{}) {
    n += m(1) + auto{m}(2);
  }
  return n;
}
