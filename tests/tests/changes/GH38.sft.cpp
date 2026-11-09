//type:fn
//options:--gnu_version 150200
//require:INT128_EXTENSIONS_ALLOWED 1
//remark:[GH #38] Integer value limits for a 128-bit host integer representation
// 10/20/26 [GH #38]
//
// Constant arithmetic on __int128 values that do not fit in 64 bits is
// accepted, and overflow of the 128-bit range is still diagnosed.

constexpr __int128 s = (__int128)1 << 100;
static_assert(s * 4 == ((__int128)1 << 102), "");
static_assert(-s * 4 == -((__int128)1 << 102), "");

constexpr unsigned __int128 u = (unsigned __int128)1 << 100;
static_assert(u * 4 == ((unsigned __int128)1 << 102), "");
static_assert(u + u == ((unsigned __int128)1 << 101), "");

constexpr __int128 max = (__int128)(~(unsigned __int128)0 >> 1);
constexpr __int128 min = -max - 1;
static_assert(max / 3 * 3 + 1 == max, "");
static_assert(min + 1 == -max, "");

static_assert(max + 1 != 0, "");   // Error: overflow
static_assert(min / -1 != 0, "");  // Error: overflow
static_assert(max * 2 != 0, "");   // Error: overflow
static_assert(min * 2 != 0, "");   // Error: overflow
