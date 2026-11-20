//type:rp
//options_all:--c++11 --exceptions

int calls;
int destroyed;
int order[4];

struct Guard {
  ~Guard() { ++destroyed; }
};

struct TrivialDestructor {
  TrivialDestructor() {
    if (++calls == 3)
      throw 42;
  }
};

struct NontrivialDestructor {
  int id;
  NontrivialDestructor() : id(++calls) {
    if (id == 3)
      throw 43;
  }
  ~NontrivialDestructor() { order[destroyed++] = id; }
};

struct Holder {
  NontrivialDestructor elements[4];
  Holder();
};

Holder::Holder() {}

struct ValueMember {
  ValueMember() {
    if (++calls == 3)
      throw 44;
  }
};

struct ValueElement {
  int value;
  ValueMember member;
  ValueElement() = default;
  ~ValueElement() { ++destroyed; }
};

struct ValueHolder {
  ValueElement elements[4];
  ValueHolder() : elements{} {}
};

int main() {
  try {
    Guard guard;
    TrivialDestructor elements[4];
    return 1;
  } catch (int value) {
    if (value != 42 || calls != 3 || destroyed != 1)
      return 2;
  }
  calls = 0;
  destroyed = 0;
  try {
    Holder holder;
    return 3;
  } catch (int value) {
    if (value != 43 || calls != 3 || destroyed != 2 ||
        order[0] != 2 || order[1] != 1)
      return 4;
  }
  calls = 0;
  destroyed = 0;
  try {
    ValueHolder holder;
    return 5;
  } catch (int value) {
    if (value != 44 || calls != 3 || destroyed != 2)
      return 6;
  }
  return 0;
}
