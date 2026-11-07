//type:rp
//options_all:--c++20
//options:--g++ --gnu_version=130300:--strict
// Test the emitted C representation as well as the front-end type.
enum class Signed { zero = 0 };
enum struct Empty {};
enum class Unsigned : unsigned { zero = 0 };
enum class Boolean : bool { zero = false, one = true };
struct Fields { Signed s : 3; Unsigned u : 3; Boolean b : 1; };
static_assert(__is_same(__underlying_type(Signed), int), "signed base");
static_assert(__is_same(__underlying_type(Empty), int), "empty base");
volatile int minus_one = -1;
int main() {
  Signed s = static_cast<Signed>(minus_one);
  Empty e = static_cast<Empty>(minus_one);
  Fields f{static_cast<Signed>(minus_one), Unsigned::zero, Boolean::one};
  return static_cast<long>(s) != -1 || static_cast<long>(e) != -1 ||
         static_cast<int>(f.s) != -1 || static_cast<unsigned>(f.u) != 0 ||
         !static_cast<bool>(f.b);
}
