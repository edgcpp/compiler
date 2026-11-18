//type:cp
//options_all:--c++20
struct vector { int *first, *last; };
constexpr bool test() {
    auto v = vector{ nullptr, nullptr };
    auto p = v.last >= v.first;
    return true;
}
static_assert(test());
