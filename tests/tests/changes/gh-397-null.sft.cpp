//type:fn
//options:--g++ --c++20
//remark:[GH #397] Null pointer result of a covariant virtual call

struct E { int e = 0; };
struct X { int x = 7; };
struct Y { int y = 0; };
struct Z : Y, X {};
struct P { constexpr virtual const X* p() const { return nullptr; } };
struct S : E, P {
  Z z;
  constexpr const Z* p() const override { return nullptr; }
};
constexpr bool f() { S s; const P& rp = s; return rp.p() == nullptr; }
static_assert(f());
constexpr int g() {
  S s;
  const P& rp = s;
  return __builtin_memcmp(rp.p(), (const X*)&s.z, sizeof(X));
}
constexpr int v = g();  // Error: null pointer argument (previously crashed).
