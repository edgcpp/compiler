//type:rp
//options_all:--c++17 --exceptions

typedef decltype(sizeof(0)) Size;
namespace std {
  enum class align_val_t : Size {};
}

alignas(128) unsigned char storage[1024];
void *allocated_ptr;
Size allocated_size;
void *deallocated_ptr;
Size deallocated_size;
int deallocations;

void *allocate(Size size) {
  allocated_ptr = storage;
  allocated_size = size;
  return storage;
}
void deallocate(void *p, Size size) {
  deallocated_ptr = p;
  deallocated_size = size;
  ++deallocations;
}

void *operator new[](Size size, std::align_val_t) {
  return allocate(size);
}
void operator delete[](void *p, std::align_val_t) noexcept {
  deallocate(p, 0);
}
void operator delete[](void *p, Size size, std::align_val_t) noexcept {
  deallocate(p, size);
}

int constructions;
int throw_at;

void constructed() {
  if (++constructions == throw_at) throw 1;
}

struct alignas(64) Global {
  int value;
  Global() : value(1) { constructed(); }
  Global(int v) : value(v) { constructed(); }
  ~Global() {}
};

struct alignas(64) Member {
  int value;
  Member() : value(1) { constructed(); }
  Member(int v) : value(v) { constructed(); }
  ~Member() {}
  void *operator new[](Size size, std::align_val_t) {
    return allocate(size);
  }
  void operator delete[](void *p, std::align_val_t) noexcept {
    deallocate(p, 0);
  }
};

/* Return 0 if the storage was freed exactly once using the pointer (and,
   for a sized delete, the size) that was allocated. */
int check(int (*make)(int), int count, int throw_on) {
  int result = 0;
  constructions = 0;
  throw_at = throw_on;
  deallocations = 0;
  allocated_ptr = deallocated_ptr = 0;
  allocated_size = deallocated_size = 0;
  try {
    make(count);
    if (throw_on != 0) result = 1;
  } catch (int) {
    if (throw_on == 0) result = 1;
  }
  if (result == 0 && deallocations != 1) result = 2;
  if (result == 0 && deallocated_ptr != allocated_ptr) result = 3;
  if (result == 0 && deallocated_size != 0 &&
      deallocated_size != allocated_size) {
    result = 4;
  }
  return result;
}

template <class T> int make_brace(int count) {
  delete[] new T[count]{};
  return 0;
}
template <class T> int make_list(int count) {
  delete[] new T[count]{T(7)};
  return 0;
}
template <class T> int make_multi_brace(int count) {
  delete[] new T[count][3]{};
  return 0;
}
template <class T> int make_multi_list(int count) {
  delete[] new T[count][3]{{T(7)}};
  return 0;
}

template <class T> int check_all(int count, int throw_on) {
  int result;
  if ((result = check(make_brace<T>, count, throw_on)) != 0) {
    return 10 + result;
  }
  if ((result = check(make_list<T>, count, throw_on)) != 0) {
    return 20 + result;
  }
  if ((result = check(make_multi_brace<T>, count, throw_on)) != 0) {
    return 30 + result;
  }
  if ((result = check(make_multi_list<T>, count, throw_on)) != 0) {
    return 40 + result;
  }
  return 0;
}

int main(int argc, char **) {
  int count = argc + 2;
  int result;
  if ((result = check_all<Global>(count, 2)) != 0) return 50 + result;
  if ((result = check_all<Member>(count, 2)) != 0) return 100 + result;
  if ((result = check_all<Global>(count, 0)) != 0) return 150 + result;
  if ((result = check_all<Member>(count, 0)) != 0) return 200 + result;
  return 0;
}
