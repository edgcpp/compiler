//type:fp
//options_all:--microsoft_v 1929 --ms_c++latest -tused
//remark:[6.5] Spurious error in C++20 mode on initialization of aggregate
// 4/4/23   [EDGcpfe/24743,EDGcpfe/25586,EDGcpfe/26172]
//
// Spurious error in C++20 mode on initialization of aggregate
//
// Although C++20 allows parenthesized aggregate initialization, "D d(p);" is not
// such an initialization; instead, it is a direct initialization via the user-
// defined conversion from S to D.  Previously, however, the front end failed to
// a match according to the aggregate initialization rules (i.e., matching p to
// member x of D).  That is now fixed.
struct D { int x, y, z; };  // Aggregate.
struct S  { operator D() const; };
void g(S p) {
  D d(p);  // Previously an error.  Now okay.
}
