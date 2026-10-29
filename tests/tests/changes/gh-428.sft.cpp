//type:fp
//options:--c++20
//remark:Constant evaluation of a base-to-derived null pointer cast

struct X { int x = 7; };
struct Y { int y = 0; };
struct Z : Y, X {};
struct W : Y, Z {};

constexpr bool f() {
  const X* q = nullptr;
  return static_cast<const Z*>(q) == nullptr;
}
static_assert(f());  // Previously failed.

constexpr bool g() { const X* q = 0; return (const Z*)q == nullptr; }
static_assert(g());  // Previously failed.

constexpr const X* get_null() { return nullptr; }
constexpr bool h() { return static_cast<const W*>(get_null()) == nullptr; }
static_assert(h());  // Previously failed.

constexpr bool i() {
  const X* q = nullptr;
  const Z* p = static_cast<const Z*>(q);
  return static_cast<const X*>(p) == nullptr && !p;
}
static_assert(i());  // Previously failed.

constexpr bool j() {
  W w;
  const X* q = &w;
  return static_cast<const W*>(q) == &w;
}
static_assert(j());
