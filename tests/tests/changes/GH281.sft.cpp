//type:fp
//options:--c++11:--c++11 --gnu_version 160200:--c++11 --clang_version 230100:--c++20:--c++20 --gnu_version 160200:--c++20 --clang_version 230100:--ms_c++20 --microsoft_version 1950
//options_all:-tused
//remark:Explicit conversion functions before aggregate fallback [GH #281]

struct D { float x, y; };
struct S { explicit operator D() const; };
D d(S{});  // Error, now okay.
