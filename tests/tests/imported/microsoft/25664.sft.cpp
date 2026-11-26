//options_all:--ms_c++20
consteval const char *function_name(const char *name = __builtin_FUNCTION()) {
    return name;
}

int main(int, char* []) {
    constexpr auto p = function_name();
    static_assert(p[0] == 'm' && p[1] == 'a' && p[2] == 'i' && p[3] == 'n' && p[4] == 0);
}
