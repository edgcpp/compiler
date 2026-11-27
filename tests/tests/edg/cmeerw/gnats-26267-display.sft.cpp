//type:fp
//options:--c++20 --no_il_lower --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

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

/* @filter
file-scope constant
  source_corresp/name, type, coordinates.position, coordinates.depth
 */
