//type:fp
//options:--c++20 --no_il_lower --il_display
//filter:awk '/^file-scope constant@/{f=1; next}/^$/{if (f) print "|"; f=0}f{print $0 " "}' | grep -E -e '^(  name|type|coordinates.[^:]*):' -e '^[|]$' | sed -e 's/@[0-9a-f]*:/:/' -e 's/^  *//' -e 's/:  */: /' -e 's/ file-scope / /' | tr -d '\n' | tr '|' '\n' | sort

using INT = int;
using CINT = const INT;

struct X
{ };

using x_t = X;
using cx_t = const x_t;

// should keep typedefs in IL, if possible
template<int I1, const int I2, INT I3, CINT I4,
         X   X1, const X   X2, x_t X3, cx_t X4>
struct C
{ };
