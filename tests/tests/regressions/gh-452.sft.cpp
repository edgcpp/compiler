//type:fp
//options:--c++23 -A
//remark:[GH #452] Unicode 18 Jurchen and Small Seal named character escapes

static_assert(U'\N{JURCHEN CHARACTER-18E00}' == 0x18E00);
static_assert(U'\N{JURCHEN CHARACTER-19191}' == 0x19191);

static_assert(U'\N{SMALL SEAL CHARACTER-3D000}' == 0x3D000);
static_assert(U'\N{SMALL SEAL CHARACTER-3FC3F}' == 0x3FC3F);
