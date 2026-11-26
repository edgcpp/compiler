//options_all:--c++17 --clang_version 40000
//type:fp
#if __has_builtin(__make_integer_seq)
#else
#error "Behavior diverges from Clang"
#endif
