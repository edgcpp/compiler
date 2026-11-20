//options_all:--microsoft_version=1928 --ms_std_preprocessor
//type:fp
#define F(...) f(0 __VA_OPT__(,) __VA_ARGS__)
void f(int, ...);
void g() {
  F();
}
