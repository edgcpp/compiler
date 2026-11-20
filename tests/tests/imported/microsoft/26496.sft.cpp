//type:cp
//options_all:--ms_c++20 --microsoft_v 1940
template <class Ty1, class Ty2>
inline constexpr bool is_layout_compatible_v = __is_layout_compatible(Ty1, Ty2);

static_assert(is_layout_compatible_v<const int[], int[]>);
static_assert(is_layout_compatible_v<const int[3], int[3]>);
static_assert(is_layout_compatible_v<int[], volatile int[]>);
