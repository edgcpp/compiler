//type:fp
//options_all:--c++20
//options:--gnu_version 150300:--clang_version 190100

template<class T> void f(T &p)
  requires (__atomic_always_lock_free(sizeof(T), &p));
template<class T> concept lock_free = requires(T &p) { f(p); };

static_assert(lock_free<char>);
static_assert(!lock_free<char[3]>);
