//type:fn
//options:--c++26
//remark:dynamic_cast in constant evaluation with virtual base classes

// These cases cannot be constant-evaluated yet: constexpr objects of classes
// with virtual base classes are not supported (that requires P3533 "constexpr
// virtual inheritance").  When that support is added, the errors below go
// away and these assertions must pass.  In particular, a downcast must
// succeed when the operand is a public base class subobject of the
// destination object through at least one derivation path, even if another
// path to the same virtual base class subobject is not public.
struct B { virtual void f() {} };
struct Q : private virtual B {};
struct P : public virtual B {};
struct Good : public Q, public P {};
struct D : private Good {};
struct Good2 : public P, public Q {};
struct D2 : private Good2 {};

constexpr D d;
static_assert(dynamic_cast<const Good*>((const B*)&d) == (const Good*)&d);
static_assert(dynamic_cast<const P*>((const B*)&d) ==
              (const P*)(const Good*)&d);
static_assert(dynamic_cast<const D*>((const B*)&d) == nullptr);
constexpr D2 d2;
static_assert(dynamic_cast<const Good2*>((const B*)&d2) == (const Good2*)&d2);

constexpr bool local() {
  D x;
  const B* pb = (const B*)&x;
  return dynamic_cast<const Good*>(pb) == (const Good*)&x &&
         dynamic_cast<const P*>(pb) == (const P*)(const Good*)&x &&
         dynamic_cast<const D*>(pb) == nullptr;
}
static_assert(local());
