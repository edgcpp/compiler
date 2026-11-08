//type:fn
//options:--c++20:--c++20 --g++ --gnu_version=150000:--c++20 --clang --clang_version=190100
//remark:Reading an uninitialized member of a class prvalue is not constant.

// Since C++20, a constexpr constructor can leave a member uninitialized.
// Selecting the member of a class prvalue with "." or ".*" must not treat
// the whole prvalue as initialized.

struct W { int n; int m; constexpr W() : m(1) {} };
constexpr int W::*pn = &W::n;
constexpr int W::*pm = &W::m;

static_assert(W().m == 1);
static_assert((W().*pm) == 1);

static_assert(W().n == 0);      // Error.
static_assert((W().*pn) == 0);  // Error.
