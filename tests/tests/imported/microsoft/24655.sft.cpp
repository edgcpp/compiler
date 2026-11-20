//options_all:--c++14 --microsoft
struct A {
  void f() &&;
};
A g();
void h() {
  g().f();
}
