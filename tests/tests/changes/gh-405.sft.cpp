//type:fn
//options:--diag_suppress=39,62,63:--diag_suppress=39:--diag_suppress=62:--diag_suppress=63:
//options_all:--c++20
//remark:[GH #405] A suppressed note of an error no longer stops the front end
// 10/22/26  [GH #405]
//
// When constant evaluation fails, the interpreter attaches notes to the
// error "expression must have a constant value": #63 (shift count is too
// large), #62 (shift count is negative) and #39 (division by zero) here.  A
// note takes the severity of its own error number, so with --diag_suppress
// (or a diag_suppress pragma) naming one of them the note had a severity
// that cannot be displayed, and the front end stopped with an internal
// error:
//
//   internal error #2656: assertion failed: determine_severity_code:
//   bad severity
//
// A suppressed note is now left out of the diagnostic, as a suppressed
// diagnostic always is.  The error itself is still reported.
//
// The five option sets compile the same source: all three numbers
// suppressed, each one alone, and none (the notes are shown).  Every
// constant below is an error in every set; only the notes of the suppressed
// numbers are absent.

constexpr int shift_too_large = 1 << 32;
constexpr int shift_negative = 1 << -1;
constexpr int divide_by_zero = 10 / 0;

enum { E_shift_too_large = 1 << 32, E_divide_by_zero = 10 / 0 };

int array_bound[1 << 32];

static_assert((1 << 33) == 0);

template <int N> struct S {};
S<1 << 34> s;

int f(int x) {
  // Not constant expressions: only warnings, and only for the numbers that
  // are not suppressed (set 1 suppresses all three, set 5 none of them).
  return (x << 40) + (x / 0) + (x << -1);
}
