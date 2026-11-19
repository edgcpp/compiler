//type:fn
//require:THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED 1
//options:--c++17 --g++ --gnu_version=150000:--c++17 --clang --clang_version=190100
// An instance of a "__thread" variable template is thread-local, so it is
// diagnosed like a non-template "__thread" variable: GCC and Clang reject
// both of these (they were accepted before the instances were thread-local).
int f();
struct N { N(); int i; };

// A "__thread" variable cannot be dynamically initialized.
template<class T> __thread T dv = f();
template<class T> __thread T ddv;
int use() { return dv<int> + ddv<N>.i; }  // error (twice)

// The address of a thread-local variable is not a constant.
template<class T> __thread T av = 1;
constexpr int *q = &av<int>;  // error
int use_q() { return *q; }
