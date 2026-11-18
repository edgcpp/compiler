//type:cp
//options_all:--c++17
constexpr auto epsilon = 2.2204460492503131e-016;
constexpr auto max_no_loss = 1.0 / epsilon;
constexpr auto max_no_loss_int64 = (long long)max_no_loss;
static_assert(max_no_loss_int64 == 0x10'0000'0000'0000);
