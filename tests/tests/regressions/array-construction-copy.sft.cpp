//type:rp
//options_all:--c++11 --exceptions

int sequence;
int copied;
int throw_at;
int destroyed;
int order[8];

struct Element {
  int value;
  Element() : value(++sequence) {}
  Element(const Element &source) : value(source.value) {
    if (++copied == throw_at) throw 45;
  }
  ~Element() { order[destroyed++] = value; }
};

struct Holder {
  Element elements[3];
};

int main() {
  Holder source;
  throw_at = 3;
  try {
    Holder copy(source);
    return 1;
  } catch (int value) {
    if (value != 45 || copied != 3 || destroyed != 2 ||
        order[0] != 2 || order[1] != 1)
      return 2;
  }
  copied = 0;
  throw_at = 0;
  {
    Holder copy(source);
    if (copy.elements[0].value != 1 || copy.elements[1].value != 2 ||
        copy.elements[2].value != 3 || copied != 3)
      return 3;
  }
  if (destroyed != 5 || order[2] != 3 || order[3] != 2 || order[4] != 1)
    return 4;
  return 0;
}
