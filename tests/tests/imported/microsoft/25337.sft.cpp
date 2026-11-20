//options_all:--gn 110000 --c++17
int strcmp(const char* a, const char* b) { return 0; }

int main() {
    const char *foo1 = "foo";
    const char *foo2 = "foo2";
    if (__builtin_constant_p((strcmp(foo1, foo2)))) {
        __extension__({
            char __compile_time_assert__[(strcmp(foo1, foo2)) ? 1 : -1];
            (void)__compile_time_assert__;
        });
    }
}
