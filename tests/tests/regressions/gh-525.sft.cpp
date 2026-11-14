//type:rp
//options:--c++26:--c++23
//remark:Reference initialized from a reference whose referent is not known

// A reference whose initializer names a reference parameter (or any other
// reference whose referent is not known at compile time) must be bound to the
// referent.  In C++26 mode, the initializer was folded to the
// "address" of the named reference itself, so the new reference designated
// the storage of the parameter instead of the object the parameter refers to.
int fails;
void check(bool b) { if (!b) ++fails; }

struct T3 { int a; long b; char c; };
T3 g = {1, 2, 3};
T3 &gr = g;

// References to a reference parameter.
long typed_cref(const T3 &s) { const T3 &r = s; return (long)r.a + r.b + r.c; }
long auto_ref(T3 &s) { auto &r = s; return (long)r.a + r.b + r.c; }
long auto_rref(T3 &s) { auto &&r = s; return (long)r.a + r.b + r.c; }
long decltype_auto(T3 &s) {
  decltype(auto) r = (s);
  return (long)r.a + r.b + r.c;
}
long scalar_cref(const int &i) { const int &r = i; return r; }
long rvalue_ref(T3 &&s) { T3 &r = s; return (long)r.a + r.b + r.c; }
void write_ref(T3 &s) { T3 &r = s; r.a = 4; r.b = 5; r.c = 6; }
long chained(const T3 &s) {
  const T3 &r1 = s;
  const T3 &r2 = r1;
  return (long)r2.a + r2.b + r2.c;
}

// Structured bindings by reference.
long binding(const T3 &s) {
  const auto &[a, b, c] = s;
  return (long)a + b + c;
}
void write_binding(T3 &s) { auto &[a, b, c] = s; a = 7; b = 8; c = 9; }
template <typename S> long local_ref(const S *p) {
  const S &s = *p;
  const auto &[a, b, c] = s;
  return (long)a + b + c;
}
#if __cplusplus > 202302L
template <typename S> long binding_pack(const S &s) {
  const auto &[...refs] = s;
  return (0L + ... + (long)refs);
}
template <typename S> void write_pack(S &s) {
  auto &[...r] = s;
  ((r = 7), ...);
}
#endif

// Other references whose referent is not known.
extern T3 &xr;
struct W { const T3 &r; };
const T3 &ns_ref = xr;
W ns_agg = {xr};
long local_agg(const T3 &s) {
  const W &w = W{s};
  static const W sw = {s};
  return (long)w.r.a + sw.r.b + sw.r.c;
}
long static_ref(const T3 &s) {
  static const T3 &r = s;
  return (long)r.a + r.b + r.c;
}
long extern_ref() { const T3 &r = xr; return (long)r.a + r.b + r.c; }
long local_ref_nontemplate(const T3 *p) {
  const T3 &s = *p;
  const T3 &r = s;
  return (long)r.a + r.b + r.c;
}

// Cases that were handled correctly.
struct M {
  int v;
  int get() const { const M &m = *this; return m.v; }
  void set(int i) { M &m = *this; m.v = i; }
};
long from_object(T3 s) { const auto &r = s; return (long)r.a + r.b + r.c; }
long from_known_ref() {
  T3 &lr = g;
  auto &r = lr;
  return (long)r.a + r.b + r.c;
}
long from_global_ref() { auto &r = gr; return (long)r.a + r.b + r.c; }
template <typename S> long from_deref(const S *p) {
  const auto &[a, b, c] = *p;
  return (long)a + b + c;
}
#if __cplusplus > 202302L
template <typename S> long by_value_pack(const S &s) {
  auto [...v] = s;
  return (0L + ... + (long)v);
}
long constexpr_ref() {
  int x = 6;
  constexpr int &r = x;  // Okay in C++26: The referent is known.
  return r;
}
#endif

int main() {
  T3 t = {1, 2, 3};
  int i = 6;
  check(typed_cref(t) == 6);
  check(auto_ref(t) == 6);
  check(auto_rref(t) == 6);
  check(decltype_auto(t) == 6);
  check(scalar_cref(i) == 6);
  check(rvalue_ref(static_cast<T3 &&>(t)) == 6);
  check(chained(t) == 6);
  check(binding(t) == 6);
  check(local_ref(&t) == 6);
  check(extern_ref() == 6);
  check(local_ref_nontemplate(&t) == 6);
  check((long)ns_ref.a + ns_ref.b + ns_ref.c == 6);
  check((long)ns_agg.r.a + ns_agg.r.b + ns_agg.r.c == 6);
  check(local_agg(t) == 6);
  check(static_ref(t) == 6);
  check(from_object(t) == 6);
  check(from_known_ref() == 6);
  check(from_global_ref() == 6);
  check(from_deref(&t) == 6);
#if __cplusplus > 202302L
  check(binding_pack(t) == 6);
  check(by_value_pack(t) == 6);
  check(constexpr_ref() == 6);
  write_pack(t);
  check(t.a == 7 && t.b == 7 && t.c == 7);
#endif
  write_ref(t);
  check(t.a == 4 && t.b == 5 && t.c == 6);
  write_binding(t);
  check(t.a == 7 && t.b == 8 && t.c == 9);
  M m = {1};
  m.set(5);
  check(m.v == 5 && m.get() == 5);
  return fails;
}

T3 &xr = g;
