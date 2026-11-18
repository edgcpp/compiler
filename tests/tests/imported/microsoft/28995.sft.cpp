//type:cp
//options_all:-w --microsoft_v 1950 --ms_c++latest
#pragma pack(push, 1)
struct Packed {
      char      c;
      long long d;
};

// pack(pop) is now PENDING (bound to the next token).  The warning pragmas
// mirror the CRT header preamble and keep additional entries on the list.
#pragma pack(pop)
#pragma warning(push)
#pragma warning(disable: 4324)
#if (0 + 1)
#endif
#pragma warning(pop)

struct S {
      char      c;
      long long d;
};

static_assert(sizeof(S) == 16, "Size of struct S is not 16 bytes");
