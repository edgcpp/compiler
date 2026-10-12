//type:rp
//options_all:--gnu_version=150000 --set_flag=no_checking_pragmas
//options:--g++:--clang
inline void *operator new(decltype(sizeof 0), void *p) noexcept { return p; }
int deletes;
inline void operator delete(void *, void *) noexcept { ++deletes; }

struct A { A(); };
// The code generated for Alloc::construct calls setjmp, to call the placement
// operator delete if the constructor throws.
struct Alloc { void construct(A *p) { ::new ((void *)p) A(); } };
struct Alloc2 { Alloc a; void construct(A *p) { a.construct(p); } };

[[gnu::always_inline]] inline void tconstruct(Alloc &a, A *p) {
  a.construct(p);
}

[[gnu::always_inline]] static inline void sconstruct(Alloc &a, A *p) {
  a.construct(p);   // Previously gave a gcc error.
}

[[gnu::always_inline]] static inline void s2construct(Alloc2 &a, A *p) {
  a.construct(p);   // Previously gave a gcc error.
}

void f(Alloc &a, A *p) { tconstruct(a, p); }
void g(Alloc &a, A *p) { sconstruct(a, p); }
void h(Alloc2 &a, A *p) { s2construct(a, p); }

A::A() { throw 1; }

int main() {
  alignas(A) unsigned char buf[sizeof(A)];
  A *p = (A *)buf;
  Alloc a;
  Alloc2 a2;
  try { f(a, p); } catch (int) {}
  try { g(a, p); } catch (int) {}
  try { h(a2, p); } catch (int) {}
  return deletes == 3 ? 0 : 1;
}
