//type:rp
//options_all:--c++14 --exceptions

typedef decltype(sizeof(0)) Size;

alignas(long double) unsigned char storage[256];
Size allocated_size;
Size deallocated_size;
int deallocations;

void *operator new[](Size size) {
  allocated_size = size;
  return storage;
}
void operator delete[](void *) noexcept {}
void operator delete[](void *p, Size size) noexcept {
  if (p == storage) {
    deallocated_size = size;
    ++deallocations;
  }
}

int constructions;
int throw_at;

struct Element {
  int value;
  Element() : value(1) {
    if (++constructions == throw_at) throw 1;
  }
  Element(int v) : value(v) {
    if (++constructions == throw_at) throw 1;
  }
  ~Element() {}
};

int check(int (*make)(int), int count) {
  int result = 0;
  constructions = 0;
  throw_at = 2;
  deallocations = 0;
  allocated_size = deallocated_size = 0;
  try {
    make(count);
    result = 1;
  } catch (int) {
  }
  if (result == 0 && deallocations != 1) result = 2;
  if (result == 0 && deallocated_size != allocated_size) result = 3;
  return result;
}

int make_brace(int count) {
  delete[] new Element[count]{};
  return 0;
}
int make_list(int count) {
  delete[] new Element[count]{Element(7)};
  return 0;
}
int make_multi_brace(int count) {
  delete[] new Element[count][3]{};
  return 0;
}
int make_multi_list(int count) {
  delete[] new Element[count][3]{{Element(7)}};
  return 0;
}

int main(int argc, char **) {
  int count = argc + 2;
  int result;
  if ((result = check(make_brace, count)) != 0) return 10 + result;
  if ((result = check(make_list, count)) != 0) return 20 + result;
  if ((result = check(make_multi_brace, count)) != 0) return 30 + result;
  if ((result = check(make_multi_list, count)) != 0) return 40 + result;
  return 0;
}
