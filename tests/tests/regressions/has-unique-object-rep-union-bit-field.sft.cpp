//remark:has_unique_object_representations with bit-fields in unions
//type:fp
//options_all:--c++11
//options::--g++ --gnu_version=150100:--clang --clang_version=190000:--microsoft

// An unnamed bit-field in a union overlaps the other members; it must not be
// treated as padding by itself.  A bit-field shorter than the union is
// padding except in Clang and Microsoft modes (they ignore that case).
union U1 { int x : 3; int y; };     // Short named bit-field.
union U2 { int x : 32; };           // Full-width named bit-field.
union U3 { int x; int : 32; };      // Full-width unnamed bit-field.
union U4 { int x; int : 8; };       // Short unnamed bit-field.
union U5 { int x : 3; };            // Only a short bit-field.
struct S6 { union { int x; int : 32; } u; };

#if defined(__clang__) || defined(_MSC_VER)
static_assert(__has_unique_object_representations(U1), "U1");
static_assert(__has_unique_object_representations(U4), "U4");
static_assert(__has_unique_object_representations(U5), "U5");
#else
static_assert(!__has_unique_object_representations(U1), "U1");
static_assert(!__has_unique_object_representations(U4), "U4");
static_assert(!__has_unique_object_representations(U5), "U5");
#endif
static_assert(__has_unique_object_representations(U2), "U2");
static_assert(__has_unique_object_representations(U3), "U3");
static_assert(__has_unique_object_representations(S6), "S6");
