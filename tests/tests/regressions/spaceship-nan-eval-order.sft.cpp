//type:rp
//options_all:--c++20 --g++

// Lowering of <=> must evaluate both operands exactly once, and must compare
// the same operand values in the NaN test and in the ordering tests.

#include <compare>

float nan_value() { return __builtin_nanf(""); }

int calls;
float one() { ++calls; return 1.0f; }

float g;
float set_g_to_nan() { g = nan_value(); return 0.0f; }

int main() {
  int failures = 0;

  // The left operand is evaluated when the right operand is a NaN.
  calls = 0;
  if ((one() <=> nan_value()) != std::partial_ordering::unordered) {
    failures |= 1;
  }  // if
  if (calls != 1) failures |= 2;

  // The right operand is evaluated when the left operand is a NaN.
  calls = 0;
  if ((nan_value() <=> one()) != std::partial_ordering::unordered) {
    failures |= 4;
  }  // if
  if (calls != 1) failures |= 8;

  // g is read either before set_g_to_nan() (less) or after it (unordered),
  // never both.
  g = 1.0f;
  std::partial_ordering r = set_g_to_nan() <=> g;
  if (r != std::partial_ordering::less &&
      r != std::partial_ordering::unordered) {
    failures |= 16;
  }  // if
  g = 1.0f;
  r = g <=> set_g_to_nan();
  if (r != std::partial_ordering::greater &&
      r != std::partial_ordering::unordered) {
    failures |= 32;
  }  // if

  // Finite operands give less, equivalent, and greater, both with operands
  // that are copied to temporaries (one()) and with one that is not (g).
  calls = 0;
  g = 2.0f;
  if ((one() <=> g) != std::partial_ordering::less) failures |= 64;
  if ((one() <=> one()) != std::partial_ordering::equivalent) failures |= 64;
  if ((g <=> one()) != std::partial_ordering::greater) failures |= 64;
  if (calls != 4) failures |= 128;

  return failures;
}
