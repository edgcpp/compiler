//type:rp
//options_all:--c++11
//options:--exceptions:--no_exceptions

typedef decltype(sizeof(0)) Size;

void *operator new(Size, void *p) { return p; }
void operator delete(void *, void *) {}

int destructed;

struct Member {
  int value;
  Member() : value(42) {}
};

struct Element {
  int value;
  int Member::*member_pointer;
  Member member;
  Element() = default;
  ~Element() { ++destructed; }
};

struct Container {
  Element elements[2][3];
  Container() : elements{} {}
};

int main() {
  alignas(Container) unsigned char storage[sizeof(Container)];
  for (Size i = 0; i != sizeof(storage); ++i)
    storage[i] = 0x5a;
  Container *container = new (storage) Container;
  for (int i = 0; i != 2; ++i) {
    for (int j = 0; j != 3; ++j) {
      Element &element = container->elements[i][j];
      if (element.value != 0 || element.member_pointer != 0 ||
          element.member.value != 42)
        return 1;
    }
  }
  container->~Container();
  return destructed == 6 ? 0 : 2;
}
