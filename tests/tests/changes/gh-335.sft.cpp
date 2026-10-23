//type:fp
//options:--c++11
//remark:[GH #335] Type transformations of qualified function types

using F = void() const;
using F = __add_pointer(F);  // Previously a spurious error, now okay.
