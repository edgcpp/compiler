//type:fp
//options:--c++20
//remark:Constant evaluation of a for loop entered through a case label

constexpr int into_for(int k) {
  bool b = false;
  int z = 0;
  switch (k) {
    for (; b;) {
      return 5;
    case 1:
      b = true;
      z = 9;
    }
    return z;
  }
  return -1;
}
static_assert(into_for(1) == 5);  // Previously failed.
static_assert(into_for(0) == -1);

constexpr int into_nested_for(int k) {
  bool b = false;
  int z = 0;
  switch (k) {
  case 0:
    if (false) {
      for (; b;) {
        return 5;
      case 1:
        b = true;
        z = 9;
      }
      return z;
    }
    return z;
  }
  return -1;
}
static_assert(into_nested_for(1) == 5);  // Previously failed.
static_assert(into_nested_for(0) == 0);

constexpr int into_while(int k) {
  int n = 0;
  switch (k) {
    while (n < 3) {
    case 1:
      ++n;
    }
    return n * 10;
  }
  return -1;
}
static_assert(into_while(1) == 30);

constexpr int into_do(int k) {
  int n = 0;
  switch (k) {
    do {
    case 1:
      ++n;
    } while (n < 4);
    return n * 10;
  }
  return -1;
}
static_assert(into_do(1) == 40);
