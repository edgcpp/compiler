//remark:has_unique_object_representations with a base that has no direct fields
//type:fp
//options_all:--c++17
//options:--g++ --gnu_version=150100:--clang --clang_version=190000::--microsoft

// B declares no fields but is not empty (it inherits A::i), so it must not
// be skipped as an empty base when checking C.
struct A { char i; };
struct B : A {};
struct C : B {};
struct D : C {};
struct E {};
struct F : E, B {};          // Real empty base is still skipped.
struct G : B { char j; };
struct P { int i; char c; }; // Tail padding.
struct Q : P {};

static_assert(__has_unique_object_representations(B), "B");
static_assert(__has_unique_object_representations(C), "C");
static_assert(__has_unique_object_representations(D), "D");
static_assert(__has_unique_object_representations(F), "F");
static_assert(__has_unique_object_representations(G), "G");
static_assert(!__has_unique_object_representations(P), "P");
static_assert(!__has_unique_object_representations(Q), "Q");
