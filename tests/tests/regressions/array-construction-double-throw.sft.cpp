//type:rp
//options_all:--c++11 --exceptions

#include <exception>

extern "C" void _Exit(int);

int constructed;
int destroyed;
int last_destroyed;

void terminate_handler() {
  _Exit(constructed == 3 && destroyed == 1 && last_destroyed == 2 ? 0 : 3);
}

struct Element {
  int id;
  Element() : id(++constructed) {
    if (id == 3) throw 1;
  }
  ~Element() noexcept(false) {
    ++destroyed;
    last_destroyed = id;
    if (id == 2) throw 2;
  }
};

struct Holder {
  Element elements[4];
};

int main() {
  std::set_terminate(terminate_handler);
  try {
    Holder holder;
  } catch (...) {
    return 1;
  }
  return 2;
}
