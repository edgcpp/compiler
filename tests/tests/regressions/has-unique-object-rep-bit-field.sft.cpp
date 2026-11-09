//remark:has_unique_object_representations with unnamed bit-fields
//type:fp
//options_all:--c++11
//options::--g++ --gnu_version=90400:--g++ --gnu_version=100400:--g++ --gnu_version=100500:--g++ --gnu_version=110300:--g++ --gnu_version=110400:--g++ --gnu_version=120200:--g++ --gnu_version=120300:--g++ --gnu_version=150100:--clang --clang_version=160000:--clang --clang_version=170000:--microsoft

// Standard behavior: an unnamed bit-field of nonzero width is padding.
// Older g++ and Clang versions did not treat it as padding.
struct S { char a; char : 8; char b; };
struct T { char a; char b; char : 8; };  // Unnamed bit-field at the end.
struct U : S {};                          // Via a base class.
struct V { S s; };                        // Via a member.
struct N { char a; char x : 8; char b; }; // Named: not padding.
struct Z { char a; char : 0; char b; };   // Zero-width: not padding.

#if defined(__GNUC__) && !defined(__clang__)
#define GNU_VER (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#else
#define GNU_VER 0
#endif
#if (GNU_VER != 0 && (GNU_VER < 100500 ||                          \
                      (GNU_VER >= 110000 && GNU_VER < 110400) ||   \
                      (GNU_VER >= 120000 && GNU_VER < 120300))) || \
    (defined(__clang__) && __clang_major__ < 17)
static_assert(__has_unique_object_representations(S), "S");
static_assert(__has_unique_object_representations(T), "T");
static_assert(__has_unique_object_representations(U), "U");
static_assert(__has_unique_object_representations(V), "V");
#else
static_assert(!__has_unique_object_representations(S), "S");
static_assert(!__has_unique_object_representations(T), "T");
static_assert(!__has_unique_object_representations(U), "U");
static_assert(!__has_unique_object_representations(V), "V");
#endif
static_assert(__has_unique_object_representations(N), "N");
static_assert(__has_unique_object_representations(Z), "Z");
