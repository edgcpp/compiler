//options_all:--c++17 --microsoft
//type:fp
constexpr bool verify(const int* const& left, const int* const& right) {
    if (!(left <= right)) return false;
    return true;
}

constexpr const int* ptr = nullptr;
static_assert(verify(ptr, ptr));
