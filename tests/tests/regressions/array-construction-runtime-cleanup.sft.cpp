//type:rp
//options_all:--c++11 --exceptions

typedef decltype(sizeof(0)) Size;

int constructed;
int destroyed;
int order[8];
int deallocated;
alignas(long double) unsigned char storage[1024];

void *operator new[](Size) { return storage; }
void operator delete[](void *p) noexcept {
  if (p == storage) ++deallocated;
}

struct Dynamic {
  int id;
  static void *operator new[](Size) { return storage; }
  static void operator delete[](void *p) {
    if (p == storage) ++deallocated;
  }
  Dynamic() : id(++constructed) {
    if (id == 3) throw 41;
  }
  ~Dynamic() { order[destroyed++] = id; }
};

struct ThrowingDestructor {
  int id;
  ThrowingDestructor() : id(++constructed) {}
  ~ThrowingDestructor() noexcept(false) {
    order[destroyed++] = id;
    if (id == 3) throw 42;
  }
};

struct DynamicDelete {
  int id;
  static void *operator new[](Size) { return storage; }
  static void operator delete[](void *p) {
    if (p == storage) ++deallocated;
  }
  DynamicDelete() : id(++constructed) {}
  ~DynamicDelete() noexcept(false) {
    order[destroyed++] = id;
    if (id == 3) throw 43;
  }
};

struct DefaultDynamicDelete {
  int id;
  DefaultDynamicDelete() : id(++constructed) {}
  ~DefaultDynamicDelete() noexcept(false) {
    order[destroyed++] = id;
    if (id == 3) throw 44;
  }
};

int main() {
  try {
    (void)new Dynamic[4];
    return 1;
  } catch (int value) {
    if (value != 41) return 21;
    if (constructed != 3) return 22;
    if (destroyed != 2) return 23;
    if (order[0] != 2 || order[1] != 1) return 24;
    if (deallocated != 1) return 25;
  }
  constructed = 0;
  destroyed = 0;
  try {
    ThrowingDestructor elements[4];
  } catch (int value) {
    if (value != 42 || constructed != 4 || destroyed != 4 ||
        order[0] != 4 || order[1] != 3 || order[2] != 2 || order[3] != 1)
      return 3;
  }
  if (destroyed != 4) return 4;
  constructed = 0;
  destroyed = 0;
  deallocated = 0;
  try {
    DynamicDelete *elements = new DynamicDelete[4];
    delete[] elements;
    return 5;
  } catch (int value) {
    if (value != 43 || constructed != 4 || destroyed != 4 ||
        order[0] != 4 || order[1] != 3 || order[2] != 2 || order[3] != 1 ||
        deallocated != 1)
      return 6;
  }
  constructed = 0;
  destroyed = 0;
  deallocated = 0;
  try {
    DefaultDynamicDelete *elements = new DefaultDynamicDelete[4];
    delete[] elements;
    return 7;
  } catch (int value) {
    if (value != 44 || constructed != 4 || destroyed != 4 ||
        order[0] != 4 || order[1] != 3 || order[2] != 2 || order[3] != 1 ||
        deallocated != 1)
      return 8;
  }
  return 0;
}
