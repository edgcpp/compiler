//type:cp
//options_all:--c++11 --gnu=80000

struct A {
  explicit operator int() { return int{}; }
};

struct B {
  explicit operator const int&() { return int{}; }
};

struct C {
  explicit operator int&&() { return int{}; }
};

#define TEST_CONV(X) { \
  int ptr{ X{} }; \
  const int& rptr{ X{} }; \
  int&& rrptr{ X{} }; \
}

void f() {
  TEST_CONV(A);
  TEST_CONV(B);
  TEST_CONV(C);
}
