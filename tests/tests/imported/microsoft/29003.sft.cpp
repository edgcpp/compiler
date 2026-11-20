//type:cp
//options_all:--microsoft_v 1951 --ms_c++latest
#define ATTACH_PRAGMA _Pragma("pack(push, 1)")

#if (0 + ATTACH_PRAGMA 1)
#endif

struct S {
char      c;
long long d;
};

static_assert(sizeof(S) == 9, "Size of struct S is not 9 bytes");
