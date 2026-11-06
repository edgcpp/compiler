//type:fp
//options_all:--set_flag=constexpr_exceptions
//options:--c++20:--c++26 --clear_flag=constexpr_exceptions;fn:--c++17
//remark:P3068R6: --set_flag/--clear_flag=constexpr_exceptions
// Exceptions can be thrown during constant evaluation in C++20 mode with
// --set_flag=constexpr_exceptions, and not in C++26 mode with
// --clear_flag=constexpr_exceptions (the second case is expected to fail).
// In C++17 mode, --set_flag=constexpr_exceptions also allows the try-block
// in the constexpr function.

constexpr int f(int n) {
  try {
    if (n < 0) throw n;
    return n;
  } catch (int i) {
    return -i;
  }
}
static_assert(f(-3) == 3);
static_assert(__cpp_constexpr_exceptions == 202411L);
