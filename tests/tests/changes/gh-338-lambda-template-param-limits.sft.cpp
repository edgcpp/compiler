//type:fn
//options_all:--c++20
//cases:4
//remark:[GH #338] What a lambda that names a template parameter cannot do yet
//match_regex:(line 34: error: identifier\s+"T"\s+is\s+undefined|line 39: error: no\s+instance\s+of\s+function\s+template\s+"f"\s+matches\s+the\s+argument\s+list|line 42: error: identifier\s+"Ts"\s+is\s+undefined|line 47: error: identifier\s+"T"\s+is\s+undefined)
// 10/16/26  [GH #338]
//
// See gh-338-lambda-template-param.  The limits of the change are pinned here;
// both declarations are valid for Clang and GCC.
//
// Case 1: the template parameters are bound for the rescan of one lambda at a
// time, and only for a lambda that is not generic.  The first lambda of the
// signature is rescanned with T bound, and the second (generic) one must not
// see that binding afterwards: it is rescanned as before, and the name T is
// not found.
//
// Case 2: a lambda is rescanned with the template parameters bound only if one
// of its tokens is the name of a template parameter.  This one depends on T
// only through decltype of a parameter, so it is rescanned as before and the
// call does not find an instance.
//
// Case 3: a lambda that names a template parameter pack is rescanned as before
// (binding the symbol of a pack is not enough for a pack expansion to find its
// elements, and that is not implemented yet), and the name is not found.
//
// Case 4: the binding of the parameters for the rescan of a lambda that is
// bound does not leak into the next lambda of the declaration: the second
// lambda names a template parameter pack, so it is rescanned as before, and
// the names T and Ts are not found (the first lambda is rescanned with T
// bound).
#if TEST_NUMBER == 1
template <class T>
void f(char (*)[sizeof([]{ return T{}; }())],
       char (*)[sizeof([](auto x){ return T(x); }(1))]) {}
void use() { char a[sizeof(int)]; f<int>(&a, &a); }
#elif TEST_NUMBER == 2
template <class T>
void f(T x, char (*)[sizeof([](decltype(x) y){ return y; }(x))]) {}
void use() { char a[sizeof(long long)]; f(1LL, &a); }
#elif TEST_NUMBER == 3
template <class... Ts>
void f(char (*)[([]{ return (sizeof(Ts) + ... + 0); }())]) {}
void use() { char a[sizeof(int) + sizeof(long long)]; f<int, long long>(&a); }
#elif TEST_NUMBER == 4
template <class T, class... Ts>
void f(char (*)[sizeof([]{ return T{}; }())],
       char (*)[([]{ return sizeof(T) + (sizeof(Ts) + ... + 0); }())]) {}
void use() { char a[sizeof(int)], b[sizeof(int) + sizeof(long long)]; f<int, long long>(&a, &b); }
#endif
