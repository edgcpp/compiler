template <class T> struct A {
  int f() { return 1; }
  int g(int n, ...) { return n; }
};
extern template struct A<int>;
