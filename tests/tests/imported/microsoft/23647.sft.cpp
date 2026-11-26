//options_all:--clang --c++17
constexpr bool EnsureConstexpr(const char *s) { return true; }

struct FormatSpecTemplate {
    template <typename = void>
    FormatSpecTemplate(const char* s) __attribute__((enable_if(EnsureConstexpr(s), ""))) {}
};

void f() {
    FormatSpecTemplate("");
}
