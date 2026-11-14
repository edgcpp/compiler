//type:fn
//options:--c++11:--c++20:--g++ --c++17 --gnu_version=80500:--g++ --c++17 --gnu_version=130300:--g++ --c++17 --gnu_version=150200:--clang --c++17 --clang_version=180100:--ms_c++17 --microsoft_version 1928:--ms_c++17 --microsoft_version 1944:--ms_c++20 --microsoft_version 1944
//remark:Explicit conversion functions and list-initialized reference temps

// Core issue 2267: when a reference is list-initialized from an element whose
// type is not reference-related, the temporary is copy-list-initialized, so
// explicit conversion functions cannot be used even for direct-list-
// initialization.  Clang, g++ before 9, and MSVC in permissive mode (the
// default before C++20) accept the direct-list-initialization cases anyway,
// and g++ 14 and later accept the one with a conversion function returning a
// reference, but no compiler accepts copy-list-initialization.

struct E { explicit operator int() const; } e;
struct ER { explicit operator int&&(); };
struct NE { operator int() const; } ne;

void h(int &&);
void hc(const int &);

// Direct-list-initialization: errors in strict mode only.
int &&r1{e};
const int &r2{e};
int &&r3{ER{}};

// Copy-list-initialization: always errors.
int &&r4 = {e};
int &&r5 = {ER{}};
void g() {
  h({e});
  h({ER{}});
  hc({e});
}

// Non-explicit conversion: okay.
int &&r6{ne};
void k() { h({ne}); }
