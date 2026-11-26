//type:rp
//options:--c++14:--c++20:--c++23:--c++14 --g++ --gnu_version=90000:--c++20 --g++ --gnu_version=150000:--c++14 --clang_version 70000:--c++20 --clang_version 230100:--ms_c++20 --microsoft_version 1950
// Subscripting an array prvalue or xvalue, such as "A().i[0]", produces an
// xvalue (Core issue 1213).  So a conditional expression whose operands are
// "A().b" and "A().i[0]" is an xvalue too, and binding a reference to it
// extends the lifetime of the temporary in the branch that was taken.
// Previously "A().i[0]" was an lvalue, the conditional was a prvalue, and the
// reference was bound to a copy of the selected int.  MSVC still yields an
// lvalue for the subscript, so in Microsoft mode only a conditional whose
// operands are both lvalues is a glvalue.  Each failing check prints its line
// number.
extern "C" int printf(const char *, ...);
int fails;
void check_(bool b, int line) {
  if (!b) {
    printf("check on line %d failed\n", line);
    ++fails;
  }
}
#define check(b) check_(b, __LINE__)

#ifdef _MSC_VER
const bool SUB_IS_XVALUE = false;
#else
const bool SUB_IS_XVALUE = true;
#endif
// Number of temporaries destroyed while a reference bound to a conditional
// expression with an "A().b" operand and an "A().i[0]" operand is alive: none
// when the conditional is an xvalue, one when it is a prvalue (a copy).
const int MIXED = SUB_IS_XVALUE ? 0 : 1;

int d;  // Number of A objects destroyed.
struct A {
  int b;
  int i[2];
  ~A() { ++d; }
};
int g = 5;

void test(bool flag) {
  d = 0;
  {
    const int &r = flag ? A().b : A().i[0];
    check(d == MIXED);  // Previously destroyed before the check.
  }
  check(d == 1);
  d = 0;
  {
    const int &r = flag ? A().i[1] : 0[A().i];
    check(d == 0);
  }
  check(d == 1);
  // With an lvalue as the other operand, the conditional is a prvalue: the
  // reference binds to a copy and the temporary is destroyed at once.
  // Previously both operands were lvalues and the temporary was extended.
  d = 0;
  {
    const int &r = flag ? A().i[0] : g;
    check(d == (flag && SUB_IS_XVALUE ? 1 : 0));
  }
}

template <class U> void test_template(bool flag) {
  d = 0;
  {
    const int &r = flag ? U().b : U().i[0];
    check(d == MIXED);  // Previously destroyed before the check.
  }
  check(d == 1);
}

struct S {
  int a[2];
  ~S() { ++d; }
};

int main() {
  test(true);
  test(false);
  test_template<A>(true);
  test_template<A>(false);
  // Binding directly to the subscript extends the lifetime too.
  d = 0;
  {
    const int &r = A().i[0];
    check(d == 0);  // Previously destroyed before the check.
  }
  check(d == 1);
#ifndef _MSC_VER
  d = 0;
  {
    int &&r = S{{1, 2}}.a[0];  // Previously an error.
    check(d == 0 && r == 1);
  }
  check(d == 1);
#endif
  return fails;
}
