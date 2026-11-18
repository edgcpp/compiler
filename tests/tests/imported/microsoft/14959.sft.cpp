//options_all:--microsoft --c++14
template <typename T> inline __declspec(deprecated("bad")) char * __cdecl _cgets(T&) throw() {
    return 0;
}

template <> inline __declspec(deprecated("bad")) char * __cdecl _cgets(char *&) throw() {
    return 0;
}
