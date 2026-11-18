//type:fp
//options_all:--clang --clang_version 30800 --ms_compatibility
void f() {
    unsigned long result;
    if (__builtin_mul_overflow(1, 1, &result)) {
    }
}
