//type:fp
//options:--c++20
//remark:Constant evaluation: use of a variable after a jump to a case label

constexpr int after_label(int n) {
  switch (n) {
  case 0:
    int x;
  case 1:
    x = 4;
    return x;
  }
  return 0;
}
static_assert(after_label(1) == 4);  // Previously failed.
static_assert(after_label(0) == 4);  // Previously failed.

constexpr int into_for() {
  switch (1) {
    for (int x; ;) {
    case 1:
      x = 3;
      return x;
    }
  }
  return 0;
}
static_assert(into_for() == 3);  // Previously failed.

constexpr int into_for_loop(bool b) {
  switch (1) {
    for (int x; b;) {
      return x;
    case 1:
      x = 43;
      b = true;
    }
  }
  return 0;
}
static_assert(into_for_loop(false) == 43);  // Previously failed.

constexpr int into_if() {
  switch (1) {
    if (int x; false) {
    } else {
    case 1:
      x = 42;
      return x;
    }
  }
  return 0;
}
static_assert(into_if() == 42);  // Previously failed.
