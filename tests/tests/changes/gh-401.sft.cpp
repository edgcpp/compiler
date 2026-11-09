//type:fp
//options:--c++20 --gnu_version 130000
//remark:Constant evaluation of break/continue in a GNU statement expression

constexpr int brk(int v) {
  int n = 0;
  for (int i = 0; i < 5; ++i) {
    n += ({ if (i == v) break; i; });
  }
  return n;
}
static_assert(brk(3) == 3);  // Previously failed.
static_assert(brk(0) == 0);  // Previously failed.
static_assert(brk(9) == 10);

constexpr int cont(int v) {
  int n = 0;
  for (int i = 0; i < 5; ++i) {
    n += ({ if (i == v) continue; i; });
  }
  return n;
}
static_assert(cont(3) == 7);  // Previously failed.

constexpr int sw(int v) {
  int n = 0;
  switch (v) {
  case 1:
    n = ({ if (v) break; 5; });
    n += 10;
  }
  return n;
}
static_assert(sw(1) == 0);  // Previously failed.

constexpr int ret(int v) {
  int m = ({ if (v) return 100; 7; });
  return m;
}
static_assert(ret(1) == 100);
static_assert(ret(0) == 7);

struct D {
  int* p;
  constexpr ~D() { ++*p; }
};
constexpr int dtors() {
  int n = 0;
  while (true) {
    D d{&n};
    int k = ({ D e{&n}; if (n == 0) break; 1; });
    n += k;
  }
  return n;
}
static_assert(dtors() == 2);  // Previously failed.

// Pointer variables (here with union member addresses) that a break leaves
// without a value, or that belong to an outer recursive activation, must not
// be cleaned up when the block is left.
union U { int a; float b; };
constexpr int ptr_in_stmt_expr(int k) {
  U u{2};
  int r = 0;
  for (int i = 0; i < 4; ++i) {
    int *p = ({ if (i == k) break; &u.a; });
    r += *p;
  }
  return r;
}
static_assert(ptr_in_stmt_expr(2) == 4);

constexpr int ptr_assigned_later(int k) {
  U u{3};
  int r = 0;
  for (int i = 0; i < 4; ++i) {
    int *p;
    if (i == k) break;
    p = &u.a;
    r += *p;
  }
  return r;
}
static_assert(ptr_assigned_later(2) == 6);

constexpr int ptr_recursive(int n) {
  U u{1};
  int r = 0;
  for (int i = 0; i < 1; ++i) {
    if (n == 0) break;
    int *p = &u.a;
    r = ptr_recursive(n - 1);
    r += *p;
  }
  return r;
}
static_assert(ptr_recursive(3) == 3);
