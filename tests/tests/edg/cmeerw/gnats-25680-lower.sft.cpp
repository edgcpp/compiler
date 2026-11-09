//type:fp
//options:--c++17 --g++:--c++17 --g++ -DARRAY:--c++17 --clang -DATOMIC:--c++17 --clang -DATOMIC -DARRAY
//options_all:--il_display
//filter:sed -e 's/@[0-9a-f]*//' | grep -E '^constant:                file-scope constant: ([(][^)]+[)])?[0-9]+UL?$' | sed -e 's/\([0-9]\+\)UL\?/\1/'

// testing note: check that lowering generates the expected implicit arguments
// (sizes and alignment) for "operator new" and "operator delete"

namespace std
{
  using size_t = decltype(sizeof 0);

  struct nothrow_t { };
  const nothrow_t nothrow = {};
  enum class align_val_t : size_t {};
}

void *operator new(std::size_t);
void operator delete(void*) noexcept;

void *operator new(std::size_t, const std::nothrow_t&);
void operator delete(void*, const std::nothrow_t&) noexcept;

void *operator new(std::size_t, void*);
void *operator new(std::size_t, std::align_val_t);
void *operator new(std::size_t, std::align_val_t, const std::nothrow_t&);

void operator delete(void*, std::align_val_t) noexcept;
void operator delete(void*, std::align_val_t, const std::nothrow_t&) noexcept;

void operator delete(void*, void*) noexcept;
void operator delete(void*, std::size_t) noexcept;
void operator delete(void*, std::size_t, std::align_val_t) noexcept;

void *operator new[](std::size_t);
void operator delete[](void*) noexcept;

void *operator new[](std::size_t, const std::nothrow_t&);
void operator delete[](void*, const std::nothrow_t&) noexcept;

void *operator new[](std::size_t, void*);
void *operator new[](std::size_t, std::align_val_t);
void *operator new[](std::size_t, std::align_val_t, const std::nothrow_t&);

void operator delete[](void*, std::align_val_t) noexcept;
void operator delete[](void*, std::align_val_t, const std::nothrow_t&) noexcept;

void operator delete[](void*, void*) noexcept;
void operator delete[](void*, std::size_t) noexcept;
void operator delete[](void*, std::size_t, std::align_val_t) noexcept;

struct C
{
  char arr[5];
};

#ifdef ATOMIC
using AC = _Atomic(C);
#else
typedef C __attribute__((__aligned__(32))) AC;
#endif

void foo()
{
#ifndef ARRAY
  {
    auto *p = new (std::nothrow) AC;
    delete p;
  }
#else
  {
    auto *p = new (std::nothrow) AC[2];
    delete[] p;
  }
#endif
}
