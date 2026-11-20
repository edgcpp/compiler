//options_all:--microsoft
void foo() {
    _Pragma("comment(linker, \"/EXPORT:\" __FUNCTION__ \"=\" __FUNCDNAME__)")
}
