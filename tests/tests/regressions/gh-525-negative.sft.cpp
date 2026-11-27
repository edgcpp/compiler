//type:fn
//options:--c++26:--c++23
//remark:Constexpr reference to an object that is not known

// A reference whose referent is not known at compile time (or "*this") may be
// named in a constant expression, but the object it designates is not a
// permitted result of a constant expression.  These initializations were
// accepted, with the reference bound to the storage of the named reference.
struct T3 { int a; long b; char c; };
extern T3 &xr;

long param(const T3 &s) {
  constexpr const T3 &r = s;  // Error
  return r.a;
}
struct W { const T3 &r; };
long aggregate(const T3 &s) {
  constexpr W w = {s};  // Error
  return w.r.a;
}
long extern_ref() {
  constexpr T3 &r = xr;  // Error
  return r.a;
}
struct M {
  int v;
  int get() const {
    constexpr const M &m = *this;  // Error
    return m.v;
  }
};
