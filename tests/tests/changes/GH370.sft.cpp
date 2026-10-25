//type:fn
//options:--c++26:--c++26 --gnu_version 150000:--c++26 --clang_version 200000
//remark:[GH #370] C++26: comparison between two arrays is not allowed
//
// P2865 makes a comparison between two arrays ill-formed in C++26.

int a[2], b[2];
int *p = b;

bool x1 = a == b;  // Two arrays: diagnosed.
bool x2 = a != b;
bool x3 = a < b;
bool x4 = a >= b;

bool y1 = a == p;  // Array and pointer: not diagnosed.
bool y2 = p < a;
