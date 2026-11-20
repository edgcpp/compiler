//type:fp
//options_all:--c++14
constexpr int f() {
 unsigned long long value = 0x00000000'FFFFFFF8ull;
 return static_cast<int>(value & 0xFFFFFFFF);
}
static_assert(f() == -8, "");
