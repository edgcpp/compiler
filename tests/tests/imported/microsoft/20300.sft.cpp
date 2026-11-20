//type:fp
//options_all:--microsoft -E
//
// This results in two tokens L and #str
// the traditional preprocessor incorrectly combined L with the
// result of #str to form a wide string literal, with a conformant
// preprocessor it will be two tokens.
#define L_STRING(str) L#str

#define u8_STRING(str) u8#str

// All of these below are good:
#define MAKE_WIDE(str) L##str
#define STRING_EXAMPLE1(str) MAKE_WIDE(#str)

#define STRING_EXAMPLE2(str) L""#str

#define STRING_EXAMPLE3(str) L###str
void* a = L_STRING(abc);
void* b = u8_STRING(def);
void* c = STRING_EXAMPLE1(ghi);
void* d = STRING_EXAMPLE2(jkl);
void* e = STRING_EXAMPLE3(mno);
