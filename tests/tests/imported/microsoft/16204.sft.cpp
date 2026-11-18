//options_all:--microsoft --c++14
struct B {
  void f() {}
};
struct C : B {};
struct D : B {};
struct E : C, D {
  using D::f;
};
void g() {
  E e;
  e.f();
}
