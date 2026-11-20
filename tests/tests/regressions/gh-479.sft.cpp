//type:rp
//options:--c++11 --g++ --gnu_version=150200:--c++20 --clang --clang_version=210000:--ms_c++20 --microsoft_version 1950
// A base class whose alignment requirement leaves padding at the end of the
// base class subobject must have that padding accounted for when IL lowering
// places the members of a derived class.  The offset of "d" in the lowered
// code must match the offset computed by the front end.

struct B { alignas(16) char c; };
struct D : B { char d; };

int main() {
  D x;
  long fe_offset = __builtin_offsetof(D, d);  // Front end's layout.
  long be_offset = (char *)&x.d - (char *)&x;  // Lowered C's layout.
  return fe_offset != be_offset;
}
