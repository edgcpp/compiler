//type:fp
//options:--gn 130000 --c++20
//remark:Control transfers and destruction order during constant evaluation

struct D {
  int *p;
  constexpr D(int *q) : p(q) {}
  constexpr ~D() { ++*p; }
};

constexpr int loops() {
  int n = 0;
  for (int i = 0; i < 10; ++i) {
    if (i == 2) continue;
    if (i == 7) break;
    n += i;
  }
  int j = 0;
  while (true) {
    if (++j > 5) break;
    if (j % 2) continue;
    n += 100;
  }
  do {
    if (j == 8) continue;
    n += 1000;
  } while (++j < 10);
  int a[] = {1, 2, 3, 4};
  for (int x : a) {
    if (x == 2) continue;
    if (x == 4) break;
    n += 10000 * x;
  }
  return n;
}
static_assert(loops() == 19 + 200 + 3000 + 40000);

constexpr int duff(int count) {
  int n = 0, k = (count + 3) / 4;
  switch (count % 4) {
    case 0: do { ++n;
    case 3:      ++n;
    case 2:      ++n;
    case 1:      ++n;
            } while (--k > 0);
  }
  return n;
}
static_assert(duff(7) == 7);
static_assert(duff(8) == 8);
static_assert(duff(1) == 1);

constexpr int sw(int v) {
  int n = 0;
  for (int i = 0; i < 3; ++i) {
    switch (v) {
      case 1: n += 1; break;
      case 2: n += 2; continue;
      case 3: if (i == 1) { case 4: n += 4; } break;
      default: return -1;
    }
    n += 10;
  }
  return n;
}
static_assert(sw(1) == 33);
static_assert(sw(2) == 6);
static_assert(sw(3) == 34);
static_assert(sw(4) == 42);
static_assert(sw(5) == -1);

constexpr int nested_switch_loop(int v) {
  int n = 0, i = 0;
  switch (v) {
    case 0:
      for (i = 0; i < 3; ++i) {
    case 1:
        n += 1;
      }
      break;
    case 2:
      while (n < 5) {
    case 3:
        ++n;
      }
  }
  return n;
}
static_assert(nested_switch_loop(0) == 3);
static_assert(nested_switch_loop(1) == 3);
static_assert(nested_switch_loop(2) == 5);
static_assert(nested_switch_loop(3) == 5);

constexpr int dtors() {
  int count = 0;
  for (int i = 0; i < 4; ++i) {
    D d(&count);
    if (i == 1) continue;
    if (i == 2) break;
  }
  {
    D d(&count);
    switch (count) {
      case 3: { D e(&count); break; }
    }
  }
  return count;
}
static_assert(dtors() == 5);

constexpr int early_return(int v) {
  int count = 0;
  {
    D d(&count);
    for (;;) {
      D e(&count);
      if (v) return count;
      break;
    }
  }
  return count;
}
static_assert(early_return(0) == 2);
static_assert(early_return(1) == 0);

constexpr int ret_in_switch(int v) {
  switch (v) { case 1: return 10; default: break; }
  return 20;
}
static_assert(ret_in_switch(1) == 10);
static_assert(ret_in_switch(2) == 20);

constexpr int no_brace_switch(int v) {
  int n = 0;
  switch (v) case 1: n = 5;
  return n;
}
static_assert(no_brace_switch(1) == 5);
static_assert(no_brace_switch(2) == 0);

constexpr int stmt_expr(int v) {
  int n = 0;
  for (int i = 0; i < 5; ++i) {
    n += ({ if (i == v) break; i; });
  }
  int m = ({ if (v) return 100 + n; 7; });
  return n + m;
}
static_assert(stmt_expr(3) == 103);
static_assert(stmt_expr(0) == 7);

struct S {
  int v;
  constexpr S(int x) : v(0) { if (x) { v = 1; return; } v = 2; }
  constexpr ~S() { if (v) return; }
};
constexpr int ctor_return() { S a(1), b(0); return a.v * 10 + b.v; }
static_assert(ctor_return() == 12);

constexpr int cond_var(int v) {
  int n = 0;
  while (int k = v--) { n += k; if (k == 2) break; }
  for (int i = 0; int k = 3 - i; ++i) { if (k == 1) continue; n += 100; }
  if (int k = n) return k;
  return -1;
}
static_assert(cond_var(4) == 209);

struct Log {
  char buf[64] = {};
  int n = 0;
  constexpr void add(char c) { buf[n++] = c; }
};

struct DL {
  Log *log;
  char c;
  constexpr DL(Log *l, char ch) : log(l), c(ch) {}
  constexpr ~DL() { log->add(c); }
};

struct B : DL {
  constexpr B(Log *l) : DL(l, 'b') {}
};

struct M : B {
  DL m1;
  DL m2;
  constexpr M(Log *l)
    : B(l), m1(l, '1'), m2(l, '2') {}
  constexpr ~M() { log->add('M'); }
};

constexpr bool eq(const char *a, const char *b) {
  while (*a && *a == *b) { ++a; ++b; }
  return *a == *b;
}

constexpr bool members() {
  Log log;
  { M m(&log); }
  return eq(log.buf, "M21b");
}
static_assert(members());

constexpr bool locals() {
  Log log;
  {
    DL a(&log, 'a');
    DL b(&log, 'b');
  }
  return eq(log.buf, "ba");
}
static_assert(locals());

constexpr int temps(Log *l) { return (DL(l, 't'), DL(l, 'u'), 1); }
constexpr bool temporaries() {
  Log log;
  int r = temps(&log);
  return r == 1 && eq(log.buf, "ut");
}
static_assert(temporaries());

constexpr bool loop_exit() {
  Log log;
  for (int i = 0; i < 3; ++i) {
    DL d(&log, (char)('0' + i));
    if (i == 1) break;
  }
  DL *p = new DL(&log, 'h');
  delete p;
  return eq(log.buf, "01h");
}
static_assert(loop_exit());

constexpr int early_return_log(Log *l) {
  DL a(l, 'a');
  {
    DL b(l, 'b');
    if (l) return 7;
  }
  return 0;
}
constexpr bool returns() {
  Log log;
  return early_return_log(&log) == 7 && eq(log.buf, "ba");
}
static_assert(returns());
