//type:rp
//options:--c++14:--c++20:--c++14 --g++ --gnu_version=90000:--c++20 --g++ --gnu_version=150000:--c++14 --clang --clang_version=40000:--c++20 --clang --clang_version=190100

// Binding a reference to a subobject of a temporary extends the lifetime of
// the whole temporary (N4140 [class.temporary]/5).  In GNU and Clang modes,
// "A().b" was a prvalue (the C++11 rule), so the reference was bound to a
// copy of the member and the A temporary was destroyed at the end of the
// full-expression.  GCC 9 and Clang 4 implement Core issue 616, under which
// "A().b" is an xvalue that designates the member of the temporary.

int d;
struct A {
  int b;
  int i[2];
  ~A() { ++d; }
};

int failures;
void check(bool ok) { if (!ok) ++failures; }

int main() {
  {
    const int &r = A().b;
    check(d == 0);   // Previously failed in GNU and Clang modes.
  }
  check(d == 1);
  d = 0;
  {
    int &&r = A().b;
    check(d == 0);   // Previously failed in GNU and Clang modes.
  }
  check(d == 1);
  d = 0;
  {
    const int &r = A().i[0];
    check(d == 0);
  }
  check(d == 1);
  d = 0;
  {
    int A::*pm = &A::b;
    const int &r = A().*pm;
    check(d == 0);
  }
  check(d == 1);
  d = 0;
  {
    int (&&r)[2] = A().i;
    check(d == 0);
  }
  check(d == 1);
  return failures;
}
