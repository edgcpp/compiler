//type:rp
//options_all:--c++14 --exceptions

typedef decltype(sizeof(0)) Size;
namespace std {
  struct nothrow_t { explicit nothrow_t() = default; };
}

alignas(long double) unsigned char storage[64];
Size allocated_size;
Size deallocated_size;
int deallocations;

void *operator new[](Size size) {
  allocated_size = size;
  return storage;
}
void *operator new[](Size size, const std::nothrow_t &) noexcept {
  allocated_size = size;
  return storage;
}
void operator delete[](void *) noexcept {}
void operator delete[](void *, const std::nothrow_t &) noexcept {}
void operator delete[](void *p, Size size) noexcept {
  if (p == storage) {
    deallocated_size = size;
    ++deallocations;
  }
}

struct Element {
  int value;
  Element() : value(1) {}
  ~Element() {}
};

struct Placed {
  int value;
  Placed() : value(1) {}
  ~Placed() {}
  static void *operator new[](Size size, int) {
    allocated_size = size;
    return storage;
  }
  static void operator delete[](void *, int) {}
  static void operator delete[](void *p, Size size) {
    ::operator delete[](p, size);
  }
};

int check() {
  int result = 0;
  if (deallocations != 1) result = 1;
  else if (deallocated_size != allocated_size) result = 2;
  deallocations = 0;
  return result;
}

int main(int argc, char **) {
  int count = argc - 1;
  int result;
  delete[] new Element[count][3];
  if ((result = check()) != 0) return 10 + result;
  delete[] new Element[count];
  if ((result = check()) != 0) return 20 + result;
  delete[] new (std::nothrow_t{}) Element[count];
  if ((result = check()) != 0) return 30 + result;
  delete[] new (1) Placed[count];
  if ((result = check()) != 0) return 40 + result;
  return 0;
}
