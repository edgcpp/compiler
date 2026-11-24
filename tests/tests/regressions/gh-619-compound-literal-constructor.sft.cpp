//type:rp
//options:--c++17 --clang:--c++17 --g++:--c++17:--ms_c++17 --microsoft

// Before the fix for GH #619, the C++-generating back end failed an assertion
// when a compound literal used as a temporary invoked a constructor.  The
// runtime checks cover chained assignment with a defaulted constructor
// argument and member access on a temporary built by an initializer-list
// constructor.
// Clang and GNU emulation exercise compound literals.  Microsoft emulation
// and plain C++17 exercise the equivalent standard list-initialization.

#include <initializer_list>

#if defined(__GNUC__) || defined(__clang__)
#define TEMPORARY(T, ...) (T){__VA_ARGS__}
#else
#define TEMPORARY(T, ...) T{__VA_ARGS__}
#endif

struct S {
  int value;
  S() : value(0) {}
  S(int x, int y, int z = 1) : value(x + y + z) {}
};

struct List {
  int sum;
  List(std::initializer_list<int> values) : sum(0) {
    for (int value : values) sum += value;
  }
};

int main() {
  S first{};
  S second{};
  first = second = TEMPORARY(S, first.value, 6);
  return first.value == 7 && second.value == 7 &&
         TEMPORARY(List, first.value, second.value).sum == 14 ? 0 : 1;
}
